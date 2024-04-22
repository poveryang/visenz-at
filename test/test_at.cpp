#include <opencv2/opencv.hpp>

#include "at_interface.h"
#include "smore_barcode.h"
#include "device_info.h"
#include <string>

#ifdef USE_SMVC
#include "cap_smvc.h"
#elif USE_V4L2
#include "cap_v4l2.h"
#endif
#include <chrono>


// Device information for different devices
std::map<std::string, DeviceInfo> dev_infos = {
        {"vs2000p", vs2000p_info},
        {"vs1000p", vs1000p_info},
        {"vs1000p200w", vs1000p_200w_info},
        {"vs2000500w", vs2000_500w_info},
        {"vs20002000w", vs2000_2000w_info}
};

float CalcSharpness(const cv::Mat &image, const std::vector<cv::Rect> &code_regions) 
{
    float acc = 0;
    for(auto region : code_regions)
    {
        // 防止越界
        region.x = std::max(0, region.x);
        region.y = std::max(0, region.y);
        if(region.br().x > image.cols-1)
        {
            region.width = image.cols - region.x;
        }
        if(region.br().y > image.rows-1)
        {
            region.height = image.rows - region.y;
        }

        cv::Mat roi_img = image(region);
        if (roi_img.cols > 1280 || roi_img.rows > 800) 
        {
            float scale = std::min(1280.0 / roi_img.cols, 800.0 / roi_img.rows);
            cv::resize(roi_img, roi_img, cv::Size(), scale, scale);
        }

        cv::Mat img_grad;
        cv::Sobel(roi_img, img_grad, CV_32FC1, 1, 1, 3);
        cv::Mat tmp1 = cv::abs(img_grad);  // 计算梯度本应该计算平方根的，但是计算绝对值的和也不会改变单调性，且速度更快
        acc += cv::sum(tmp1)[0];
    }
    
    return acc;
}

std::vector<float> FitQuadratic(const std::vector<int> &x, const std::vector<float> &y)
{
    cv::Mat A = cv::Mat::zeros(cv::Size(3, int(x.size())), CV_32FC1);
    for (int i = 0; i < x.size(); i++) {
        A.at<float>(i, 0) = x[i] * x[i];
        A.at<float>(i, 1) = x[i];
        A.at<float>(i, 2) = 1;
    }

    cv::Mat B = cv::Mat::zeros(cv::Size(1, int(y.size())), CV_32FC1);
    for (int i = 0; i < y.size(); i++) {
        B.at<float>(i, 0) = y[i];
    }

    cv::Mat tmp1 = A.t() * A;
    cv::Mat tmp2 = A.t() * B;

    cv::Mat result = cv::Mat::zeros(cv::Size(1, 3), CV_32FC1);
    cv::solve(tmp1, tmp2, result);
    float a = result.at<float>(0, 0);
    float b = result.at<float>(1, 0);
    float c = result.at<float>(2, 0);

    return {a, b, c};   // quadratic formula: y = a*x^2 + b*x + c
}

void TestATOnline(const std::string& dev_name,
                  bool enable_hmap = true,
                  bool enable_al = false,
                  bool enable_af = true,
                  bool enable_ae = true,
                  bool enable_ar = false,
                  const std::string &code_type="2d") {
    // Prepare capture object
    DeviceInfo dev_info = dev_infos[dev_name];
    CamCapture cap(dev_info.sensor_name, dev_info.sensor_width, dev_info.sensor_height, dev_info.sensor_format);

    // Initialize AT algorithm
    at::ATInterface at_obj(enable_hmap);

    // Set AR parameters and Barcode wrapper
    ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("./config/default/");
    if(dev_name == "vs1000p")
    {
        barcode_sdk.SetConfigSignature("nxp100w");
    }
    else if(dev_name == "vs1000p200w")
    {
        barcode_sdk.SetConfigSignature("nxp200w");
    }
    else if(dev_name == "vs2000500w")
    {
        barcode_sdk.SetConfigSignature("nxp500w");
    }
    else if(dev_name == "vs20002000w")
    {
        barcode_sdk.SetConfigSignature("nxp2000w");
    }
    barcode_sdk.LoadConfig("./config/default/config.json");

    if(code_type == "1d")
    {
        barcode_sdk.SetBarcodeType(smartmore::barcode::BarcodeType::kCode1D);
    }
    else if(code_type == "2d")
    {
        barcode_sdk.SetBarcodeType(smartmore::barcode::BarcodeType::kCode2D);
    }
    else
    {
        barcode_sdk.SetBarcodeType(smartmore::barcode::BarcodeType::kCode1D | smartmore::barcode::BarcodeType::kCode2D);
    }
    barcode_sdk.SetNumsMaxOutput2D(5);
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // Set enable flags of the four algorithms
    at_obj.Init(dev_info.cam_conf, barcode_wrapper, enable_al, enable_af, enable_ae, enable_ar);
    // Execute AT algorithm
    std::string at_version = at::ATInterface::GetVersion();
    printf(">>>>>===== AT version: %s <<<<<=====\n", at_version.c_str());
    printf(">>>>>===== AT has been started <<<<<=====\n");

    auto start  = std::chrono::high_resolution_clock::now();
    int iter = 0;
    bool end_iter = false;
    CamParams cam_params = at_obj.GetNextParams();

    while (!end_iter) {
        cam_params = at_obj.GetNextParams();
        cv::Mat img = cap.CapImg(cam_params);
        end_iter = at_obj.Run(img);   // at跑完的时候，会返回true
        iter += 1;
    }
    std::cout << "iter " << iter << std::endl;

    // Get final image and print best parameters
    cam_params = at_obj.GetBestParams();
    // ar_params = at_obj.GetARParams();
    cv::Mat final_img = cap.CapImg(cam_params);
    // cv::imwrite("./at_res/final.png", final_img);
    std::cout << "first stage finish" << std::endl;

    smartmore::barcode::BarcodeRequest input = {final_img};
    smartmore::barcode::BarcodeResponse output;
    auto decode_start = std::chrono::high_resolution_clock::now();
    barcode_sdk.Run(input, output);
    auto decode_end = std::chrono::high_resolution_clock::now();
    auto decode_duration = std::chrono::duration_cast<std::chrono::microseconds>(decode_end-decode_start);

    bool decode_success = false;
    std::vector<cv::Rect> code_regions;  // 计算码区的清晰度，作为评判标准
    for(const auto &result : output.results)
    {
        if(result.detect_succeed)
        {
            // author:  lushaoan
            // comment: 这个判断看起来很多余，因为result.type是由sdk设置的时候就已经确定，看起来不会返回错误的类型
            //          但是，由于pdf417的存在，当sdk.SetBarcodeType(kCode2D)的情况下，会跑一维码的分类和精定位模型
            //          就会导致在result.detect_succeed的内容里会包含一维码的结果
            //          假如场景中同时有一维码和二维码，且两者需要的焦距是不一样的话，就会导致对焦错误，因为被一维码的定位结果影响了
            if(!smartmore::barcode::IsIntersect(result.type, barcode_sdk.GetBarcodeType()))
            {
                continue;
            }

            std::vector<cv::Point> pts = {
                result.precise_locate_corner_boxes[0],
                result.precise_locate_corner_boxes[1],
                result.precise_locate_corner_boxes[2],
                result.precise_locate_corner_boxes[3]};
            
            code_regions.emplace_back(cv::boundingRect(pts));
        }
        if(result.decode_succeed)
        {
            decode_success = true;
        }
    }

    // std::cout << "code region size: " << code_regions.size() << std::endl;

    int global_focus_pos = cam_params.focus_pos;
    int best_pos = global_focus_pos;

    std::cout << "code_regions size " << code_regions.size() << std::endl;

    if (!code_regions.empty())  // 如果没有定位到码，则以全局最优对焦返回,等待后续调整曝光后再重新定位
    {
        const int range = 3; // 以global_focus_pos为中心左右各采 range 张
        int step = 5;  // step应自适应于不同机型
        std::vector<int> sampling_pos;
        std::vector<float> sampling_sharpness;
        for(int i=-range; i<=range; i++)
        {
            int pos = global_focus_pos + i * step;
            if(pos < dev_info.cam_conf.start_pos || 
                pos > dev_info.cam_conf.end_pos)
            {
                continue;
            }
            sampling_pos.emplace_back(pos);
            cam_params.focus_pos = pos;
            cv::Mat sampling_img = cap.CapImg(cam_params);
            float sharpness = CalcSharpness(sampling_img, code_regions);
            sampling_sharpness.emplace_back(sharpness);
            std::cout << "fit sharpness: " << pos << "  " << sharpness << std::endl;
        }

        if(sampling_sharpness.size() > 2)
        {
            int min_limit_pos = *std::min_element(sampling_pos.begin(), sampling_pos.end());
            int max_limit_pos = *std::max_element(sampling_pos.begin(), sampling_pos.end());
            std::cout << "fit quadratic" << std::endl;
            std::vector<float> coeffs = FitQuadratic(sampling_pos, sampling_sharpness);
            best_pos = -coeffs[1] / (2 * coeffs[0]);
            if (best_pos < min_limit_pos)
            {
                std::cout << "smaller than min_limit, pre is " << best_pos << std::endl;
                best_pos = min_limit_pos;
            }
            else if(best_pos > max_limit_pos)
            {
                std::cout << "greater than max_limit, pre is " << best_pos << std::endl;
                best_pos = max_limit_pos;
            }
            std::cout << "best_pos " << best_pos << std::endl;
            
            // float peak_sharpness = coeffs[0] * best_pos * best_pos + coeffs[1] * best_pos + coeffs[2];
            cam_params.focus_pos = static_cast<int>(round(best_pos));
        }
        else
        {
            auto max_sharpness_it = std::max_element(sampling_sharpness.begin(), sampling_sharpness.end());
            int max_sharpness_idx = std::distance(sampling_sharpness.begin(), max_sharpness_it);
            best_pos = sampling_pos[max_sharpness_idx];
            cam_params.focus_pos = static_cast<int>(round(best_pos));
        }
    }

    //
    // 下面需要分情况讨论
    // 1、 如果之前没有定位结果，就以图片亮度step=20为搜索，也就是target brightness={128, 108, 148, 88, 168, 68, 188, 48, 208}
    //     共调整9步，每拍一次图都跑一遍解码算法，直到解出码
    // 2、如果之前有定位结果，但是解码失败，就以图片亮度step=10小范围搜索，也就是 target brightness={128， 118， 138， 108， 148}
    //     共调整5步，每拍一次图都跑一遍解码算法，直到解出码
    //
    // 其实还能做得更好一些，先判断是否多码，多码的话就不是只要解出码就退出，而是loop完所有曝光，看哪个曝光解码数多就用哪个，这样更耗时就是了
    //
/*
    bool af_decode_success = false;
    cv::Mat af_img = cap.CapImg(cam_params);
    input = {af_img};
    barcode_sdk.Run(input, output);
    for(const auto &result : output.results)
    {
        if(result.decode_succeed)
        {
            af_decode_success = true;
            break;
        }
    }

    if(!af_decode_success)
    {
        std::cout << "enter ae" << std::endl;
        int range = 3;
        int init_et = cam_params.exp_time;
        int step = (dev_info.cam_conf.max_et - dev_info.cam_conf.min_et) * 0.10;
        for(int i=-range; i<=range; i++)
        {
            std::cout << "ae step " << i << std::endl;
            bool ae_finish = false;
            if(i==0)
            {
                continue;
            }
            cam_params.exp_time = init_et + i * step;
            cv::Mat img = cap.CapImg(cam_params);
            cv::imwrite("./at_res/"+std::to_string(i)+".png", img);
            input = {img};
            barcode_sdk.Run(input, output);
            for(const auto &result : output.results)
            {
                if(result.decode_succeed)
                {
                    std::cout << "ae success" << std::endl;
                    ae_finish = true;

                    cv::Mat success_canvas;
                    cv::cvtColor(img, success_canvas, cv::COLOR_GRAY2BGR);
                    cv::putText(success_canvas, result.result, result.corner_boxes[0], cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(0, 255, 0));
                    std::vector<cv::Point> pts = {
                        result.corner_boxes[0], result.corner_boxes[1],
                        result.corner_boxes[2], result.corner_boxes[3]};
                    cv::polylines(success_canvas, pts, true, cv::Scalar(0, 255, 0));
                    cv::imwrite("./at_res/success_canvas.png", success_canvas);

                    break;
                }
            }
            if(ae_finish)
            {
                break;
            }
        }
    }
*/
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "11111111111111 decode time: " << decode_duration.count() / 1000 << std::endl;
    std::cout << "22222222222222  at time: " << duration.count() / 1000 << std::endl;
    std::cout << "refine finish" << std::endl;
    final_img = cap.CapImg(cam_params);
    cv::imwrite("./at_res/final_refined.png", final_img);

    cv::Mat final_canvas;
    cv::cvtColor(final_img, final_canvas, cv::COLOR_GRAY2BGR);
    for(auto r : code_regions)
    {
        cv::rectangle(final_canvas, r, {0,255,0}, 1);
    }
    cv::imwrite("./at_res/final_canvas.png", final_canvas);

    printf(">>>>>===== AT has been ended <<<<<=====\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <device_name> [enable_hmap] [enable_al] [enable_af] [enable_ae] [enable_ar]\n", argv[0]);
        return 0;
    } else if (argc > 2) {
        std::string dev_name = argv[1];
        // bool enable_hmap = argv[2][0] == '1';
        // bool enable_al = argv[3][0] == '1';
        // bool enable_af = argv[4][0] == '1';
        // bool enable_ae = argv[5][0] == '1';
        // bool enable_ar = argv[6][0] == '1';
        std::string code_type = argv[2];

        // TestATOnline(dev_name, enable_hmap, enable_al, enable_af, enable_ae, enable_ar);
        TestATOnline(dev_name, false, false, true, false, false, code_type);
    } else {
        std::string dev_name = argv[1];
        TestATOnline(dev_name);
    }

    return 0;
}
