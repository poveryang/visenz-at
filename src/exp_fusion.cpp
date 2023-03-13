/**
 * @file ExpFusion.cpp
 * @author Junjie Yang
 * @contact: yjunjie@163.com
 * @date 2022/12/8
 * @brief 
 */

#include "exp_fusion.h"

ExpFusion::ExpFusion() {
    c_weight_ = 0.9;
    e_weight_ = 0.3;
    pyr_blending_ = true;
    pyr_layers_ = 5;
}

ExpFusion::ExpFusion(double c_weight, double e_weight, int kernel_size, bool pyr_blending, int pyr_layers) {
    c_weight_ = c_weight;
    e_weight_ = e_weight;
    pyr_blending_ = pyr_blending;
    pyr_layers_ = pyr_layers;
}

cv::Mat ExpFusion::Process(std::vector<cv::Mat> images) const {
    std::vector<cv::Mat> contrast_maps = CalcContrastMaps(images);
    std::vector<cv::Mat> exposure_maps = CalcExposureMaps(images);
    std::vector<cv::Mat> weight_maps = CalcWeightMaps(contrast_maps, exposure_maps);

    cv::Mat dst_img = PyramidBlending(images, weight_maps);
    return dst_img;
}

std::vector<cv::Mat> ExpFusion::CalcContrastMaps(std::vector<cv::Mat> &images, int kernel_size) {
    std::vector<cv::Mat> lap_images;
    cv::Mat sum_lap_images = cv::Mat::zeros(images[0].size(), CV_32FC1);
    for (const auto &image: images) {
        cv::Mat lap_image(image.size(), CV_32FC1);
        cv::Laplacian(image, lap_image, CV_32FC1, kernel_size);
        lap_image = cv::abs(lap_image) + 1e-20;
        lap_images.push_back(lap_image);
        sum_lap_images = sum_lap_images + lap_image;
    }

    std::vector<cv::Mat> contrast_maps;
    for (const auto &image: lap_images) {
        cv::Mat contrast_map;
        cv::divide(image, sum_lap_images, contrast_map, 1, CV_32FC1);
        contrast_maps.push_back(contrast_map);
    }

    return contrast_maps;
}


std::vector<cv::Mat> ExpFusion::CalcExposureMaps(std::vector<cv::Mat> &images, double sigma) {
    std::vector<cv::Mat> exposure_maps;

    cv::Mat sum_exp_maps = cv::Mat::zeros(images[0].size(), CV_32FC1);
    for (const auto &image: images) {
        cv::Mat exposure_map;
        cv::subtract(image, 0.5, exposure_map);
        cv::pow(exposure_map, 2., exposure_map);
        exposure_map = exposure_map / (2 * pow(sigma, 2));
        cv::exp(exposure_map, exposure_map);
        exposure_maps.push_back(exposure_map);
        exposure_map = cv::abs(exposure_map);
        sum_exp_maps = sum_exp_maps + exposure_map;
    }

    for (auto &exp_map: exposure_maps) {
        cv::divide(exp_map, sum_exp_maps, exp_map, 1, CV_32FC1);
    }

    return exposure_maps;
}

std::vector<cv::Mat> ExpFusion::CalcWeightMaps(std::vector<cv::Mat> &contrast_maps,
                                               std::vector<cv::Mat> &exposure_maps) const {
    std::vector<cv::Mat> weight_maps;
    for (int i = 0; i < contrast_maps.size(); i++){
        cv::Mat weight_map = cv::Mat::zeros(contrast_maps[0].size(), CV_32FC1);
        cv::multiply(contrast_maps[i], c_weight_, contrast_maps[i]);
        cv::multiply(exposure_maps[i], e_weight_, exposure_maps[i]);
        cv::add(contrast_maps[i], exposure_maps[i], weight_map);
        cv::threshold(weight_map, weight_map, 1, 1, cv::THRESH_TRUNC);
        cv::threshold(weight_map, weight_map, 0, 1, cv::THRESH_TOZERO);
        weight_maps.push_back(weight_map);
    }
    return weight_maps;
}

cv::Mat ExpFusion::PyramidBlending(std::vector<cv::Mat> images, std::vector<cv::Mat> weight_maps) const {
    /* Build Laplacian pyramid for each image, build Gaussian pyramid for each weight map*/
    std::vector<std::vector<cv::Mat>> images_lap_pyr;
    std::vector<std::vector<cv::Mat>> weights_gau_pyr;
    for (int i = 0; i < images.size(); i++){
        /* Build image Laplacian pyramid */
        cv::Mat image = images[i];

        std::vector<cv::Mat> image_gau_pyr;
        cv::buildPyramid(image, image_gau_pyr, 5);

        std::vector<cv::Mat> image_lap_pyr = {image_gau_pyr[pyr_layers_]};
        for (int j = pyr_layers_; j > 0; j--){
            cv::Mat gau_prev, gau_up;
            gau_prev = image_gau_pyr[j-1];
            cv::pyrUp(image_gau_pyr[j], gau_up);
            cv::Mat layer = gau_prev - gau_up;
            image_lap_pyr.insert(image_lap_pyr.begin(), layer);
        }
        images_lap_pyr.push_back(image_lap_pyr);
        /* Build weight Gaussian pyramid */
        cv::Mat weight = weight_maps[i];
        std::vector<cv::Mat> weight_gau_pyr;
        cv::buildPyramid(weight, weight_gau_pyr, pyr_layers_);
        weights_gau_pyr.push_back(weight_gau_pyr);
    }

    /* Weighted sum pyramid in every layer */
    std::vector<cv::Mat> weighted_sum_pyramid;
    for (int i = 0; i <= pyr_layers_; i++){
        cv::Mat weighted_sum_layer = cv::Mat::zeros(images_lap_pyr[0][i].size(), CV_32FC1);
        for (int j = 0; j < images.size(); j++){
            cv::Mat tmp;
            cv::Mat im = images_lap_pyr[j][i];
            cv::Mat we = weights_gau_pyr[j][i];
            cv::multiply(images_lap_pyr[j][i], weights_gau_pyr[j][i], tmp);
            cv::add(tmp, weighted_sum_layer, weighted_sum_layer);
        }
        weighted_sum_pyramid.push_back(weighted_sum_layer);
    }

    /* Reconstruct image from laplacian pyramid */
    cv::Mat layer = weighted_sum_pyramid[pyr_layers_];
    for (int i = pyr_layers_-1; i >=0; i--){
        cv::Mat lap_prev, lap_up;
        cv::pyrUp(layer, lap_up);
        lap_prev = weighted_sum_pyramid[i];
        layer = lap_prev + lap_up;
    }
    return layer;
}





