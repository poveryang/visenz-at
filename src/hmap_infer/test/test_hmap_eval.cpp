/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-03-13 10:15:40
 * @Copyright: Copyright (c) 2022
 */

#ifdef USE_TENGINE
#include "hmap_infer_tengine.h"
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include "json.hpp"
#include <bits/getopt_core.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>


void DfsFolder(const std::string folder_path, std::vector<std::string> &input_image_list)
{
    DIR *p_dir;
    struct dirent *ptr;

    struct stat s;
    lstat(folder_path.c_str(), &s);

    if (!S_ISDIR(s.st_mode))
    {
        std::cout << "[ERROR] not a valid directory: " << folder_path
                  << std::endl;
        return;
    }

    if (!(p_dir = opendir(folder_path.c_str())))
    {
        std::cout << "[ERROR] opendir error: " << folder_path << std::endl;
        return;
    }
    std::string sub_file;
    while ((ptr = readdir(p_dir)) != 0)
    {
        sub_file = ptr->d_name;

        std::string full_path = folder_path + "/" + sub_file;
        lstat(full_path.c_str(), &s);

        if (S_ISDIR(s.st_mode))
        {
            if (sub_file == "." || sub_file == "..")
            {
                continue;
            }

            DfsFolder(full_path, input_image_list);
        }
        else
        {
            std::string houzhui =
                sub_file.substr(sub_file.rfind(".") + 1, sub_file.size());
            if (houzhui == "json")
            {
                input_image_list.push_back(full_path);
            }
        }
    }
    closedir(p_dir);
    return;
}

void Statistics(const cv::Mat &heatmap, const std::vector<std::vector<cv::Point2f>> &gt_shapes, 
                const float intensity_thre, int heatmap_code_area_thre, float &tp, float &fp)
{
    std::vector<cv::Mat> heatmap_channels;
    cv::split(heatmap, heatmap_channels);
    cv::Mat heatmap_sum = heatmap_channels[0] + heatmap_channels[1] + heatmap_channels[2] + heatmap_channels[3];
    cv::Mat heatmap_color;
    cv::applyColorMap(heatmap_sum, heatmap_color, cv::COLORMAP_JET);

    cv::Mat heatmap_cal;
    // heatmap_sum.convertTo(heatmap_cal, CV_8UC1, 255);
    cv::cvtColor(heatmap_color, heatmap_cal, cv::COLOR_BGR2GRAY);
    double min_val, max_val;
    cv::Point min_loc, max_loc;
    cv::minMaxLoc(heatmap_cal, &min_val, &max_val, &min_loc, &max_loc);
    std::vector<cv::Point2f> hmap_centers;

    if(max_val > intensity_thre * 255)
    {
        cv::Mat heatmap_bin;
        cv::threshold(heatmap_cal, heatmap_bin, 0, 255, cv::THRESH_OTSU);
        cv::Mat labels, stats, centroids;
        int num_labels = cv::connectedComponentsWithStats(heatmap_bin, labels, stats, centroids);
        for(int i=1; i<num_labels; i++)
        {
            int component_area = stats.at<int>(i, cv::CC_STAT_AREA);
            if(component_area < heatmap_code_area_thre)
            {
                continue;
            }
            cv::Point2f component_cnt = cv::Point2f(static_cast<float>(centroids.at<double>(i, 0)), static_cast<float>(centroids.at<double>(i, 1)));
            hmap_centers.emplace_back(component_cnt);
        }
    }
    
    for(auto shape : gt_shapes)
    {
        bool match_gt = false;
        for(auto pt : hmap_centers)
        {
            if (cv::pointPolygonTest(shape, pt, false) > 0)
            {
                match_gt = true;
                break;
            }
        }

        if(match_gt)
        {
            tp += 1;
        }
        else
        {
            fp += 1;
        }
    }
}

int main(int argc, char **argv)
{
    int ch;
    std::string model_path = "../test/model/hmap-v2-qat-uint8.tmfile";
    std::string folder_path = "";
    std::string label_mode = "label";
    while ((ch = getopt(argc, argv, "m:i:l:")) != -1) {
        switch (ch) {
            case 'm':
                model_path = optarg;
                break;
            case 'i':
                folder_path = optarg;
                break;
            case 'l':
                label_mode = optarg;
                break;
            default:
                std::cout << "Usage: " << argv[0]
                          << " -c context -p precision -m model_path -i folder_path" << std::endl;
                exit(1);
        }
    }

    const float intensity_thre = 0.5;
    const int heatmap_code_area_thre = 20;
    const float resized_w = 1920, resized_h = 1200;
    float tp=0, fp=0, total_num=0, uncount=0;

    std::shared_ptr<HMapInferBase> hmap_infer;
#ifdef USE_TENGINE
    hmap_infer = std::make_shared<HMapInferTengine>();
    hmap_infer->SetInferSize(cv::Size2i(resized_w, resized_h));
    hmap_infer->SetHmapIntensityThreshold(0.2);
#endif
    hmap_infer->Init(model_path);

    if (label_mode == "label")
    {
        std::vector<std::string> file_list;
        DfsFolder(folder_path, file_list);
        if (file_list.size() == 0)
        {
            return 0;
        }

        for(auto json_file_path : file_list)
        {
            std::ifstream ifs(json_file_path);
            if (!ifs.is_open())
            {
                continue;
            }
            nlohmann::json json_info = nlohmann::json::parse(ifs);
            size_t found = json_file_path.find_last_of("/");
            std::string json_folder_path = "";
            if(found != std::string::npos)
            {
                json_folder_path = json_file_path.substr(0, found);
            }
            std::string img_path = json_folder_path + "/" + json_info["imagePath"].get<std::string>();
            cv::Mat image = cv::imread(img_path, cv::IMREAD_GRAYSCALE);
            // float scale_x = image.cols / resized_w;
            // float scale_y = image.rows / resized_h;
            std::vector<std::vector<cv::Point2f>> gt_shapes;
            for(auto shape : json_info["shapes"])
            {   
                if (shape["points"].size() != 4)
                {
                    continue;
                }
                
                cv::Point2f pt0 = cv::Point2f(shape["points"][0][0], shape["points"][0][1]);
                cv::Point2f pt1 = cv::Point2f(shape["points"][1][0], shape["points"][1][1]);
                cv::Point2f pt2 = cv::Point2f(shape["points"][2][0], shape["points"][2][1]);
                cv::Point2f pt3 = cv::Point2f(shape["points"][3][0], shape["points"][3][1]);

                std::vector<cv::Point2f> single_shape = {pt0, pt1, pt2, pt3};
                // for(auto pt : single_shape)
                // {
                //     pt.x = pt.x * scale_x;
                //     pt.y = pt.y * scale_y;
                // }
                gt_shapes.emplace_back(single_shape);
                total_num += 1;
            }

            std::cout << "img path: " << img_path << std::endl;
            cv::Mat heatmap = hmap_infer->Inference(image); // heatmap.size == image.size
            Statistics(heatmap, gt_shapes, intensity_thre, heatmap_code_area_thre, tp, fp);

            std::cout << "tp: " << tp << ",  uncount: " << uncount << ",  total_num: " << total_num << std::endl;
            std::cout << "precision: " << tp/(tp+fp) << ",  recall: " << tp/total_num << std::endl;
        }
    }
    else if(label_mode == "dl_output")
    {
        int dl_output_resolution_w = 1920;
        int dl_output_resolution_h = 1200;
        std::string json_file_path = folder_path + "/dl_output.json";
        std::ifstream ifs(json_file_path);
        nlohmann::json json_info = nlohmann::json::parse(ifs);
        for(auto decode_result : json_info["decode_results"])
        {
            std::string img_path = decode_result["image_path"];
            cv::Mat image = cv::imread(img_path, cv::IMREAD_GRAYSCALE);
            float scale_x = image.cols / dl_output_resolution_w;
            float scale_y = image.rows / dl_output_resolution_h;
            std::vector<std::vector<cv::Point2f>> gt_shapes;
            for(auto shape : decode_result["results"])
            {   
                cv::Point2f pt0 = cv::Point2f(shape["precise_locate_corner_boxes"][0][0], shape["precise_locate_corner_boxes"][0][1]);
                cv::Point2f pt1 = cv::Point2f(shape["precise_locate_corner_boxes"][1][0], shape["precise_locate_corner_boxes"][1][1]);
                cv::Point2f pt2 = cv::Point2f(shape["precise_locate_corner_boxes"][2][0], shape["precise_locate_corner_boxes"][2][1]);
                cv::Point2f pt3 = cv::Point2f(shape["precise_locate_corner_boxes"][3][0], shape["precise_locate_corner_boxes"][3][1]);

                if (pt0.x < 1 && pt2.x < 1)
                {
                    continue;
                }

                std::vector<cv::Point2f> single_shape = {pt0, pt1, pt2, pt3};
                for(auto &pt : single_shape)
                {
                    pt.x = pt.x * scale_x;
                    pt.y = pt.y * scale_y;
                }
                gt_shapes.emplace_back(single_shape);
                total_num += 1;
            }

            std::cout << "img path: " << img_path << std::endl;
            cv::Mat heatmap = hmap_infer->Inference(image);
            Statistics(heatmap, gt_shapes, intensity_thre, heatmap_code_area_thre, tp, fp);

            std::cout << "tp: " << tp << ",  uncount: " << uncount << ",  total_num: " << total_num << std::endl;
            std::cout << "precision: " << tp/(tp+fp) << ",  recall: " << tp/total_num << std::endl;
        }
    }

    return 0;
}