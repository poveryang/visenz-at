/**
 * @file ExpFusion.h
 * @author Junjie Yang
 * @contact: yjunjie@163.com
 * @date 2022/12/8
 * @brief Fusion of multiple images with different exposure into one
 */

#ifndef EXPFUSION_H
#define EXPFUSION_H

#include "opencv2/opencv.hpp"

class ExpFusion {
public:
    ExpFusion();
    explicit ExpFusion(double c_weight, double e_weight, int kernel_size, bool pyr_blending, int pyr_layers);

    ~ExpFusion() = default;

    cv::Mat Process(std::vector<cv::Mat> images) const;

    static std::vector<cv::Mat> CalcContrastMaps(std::vector<cv::Mat> &images, int kernel_size=5);

    static std::vector<cv::Mat> CalcExposureMaps(std::vector<cv::Mat> &images, double sigma=0.2);

    std::vector<cv::Mat> CalcWeightMaps(std::vector<cv::Mat> &contrast_maps, std::vector<cv::Mat> &exposure_maps) const;

    cv::Mat PyramidBlending(std::vector<cv::Mat> images, std::vector<cv::Mat> weight_maps) const;


private:
    double c_weight_;
    double e_weight_;
    bool pyr_blending_;
    int pyr_layers_;
};


#endif //EXPFUSION_H
