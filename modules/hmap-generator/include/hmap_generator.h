#ifndef HMAP_GENERATOR_HMAP_GENERATOR_H
#define HMAP_GENERATOR_HMAP_GENERATOR_H


#include "c_api.h"
#include <opencv2/opencv.hpp>

class HeatMapGenerator {
public:
    HeatMapGenerator(const std::string& context_name, const std::string& precision);
    ~HeatMapGenerator();

    void Init(const std::string& model_path);

    cv::Mat Infer(cv::Mat& image);

    double InferScore(cv::Mat& image);

    cv::Mat InferFP32(cv::Mat& image);

    cv::Mat InferUInt8(cv::Mat& image);

    cv::Rect2i FindRect(cv::Mat& image);

    double CalcScore(const cv::Mat &image);

    cv::Mat PreProcess(cv::Mat& image) const;

    cv::Mat PostProcess(cv::Mat& image) const;

    float input_scale = 0.f;
    int input_zero_point = 0;
    float output_scale = 0.f;
    int output_zero_point = 0;

private:
    graph_t graph_{};
    options opt_{};
    tensor_t input_tensor_{};
    tensor_t output_tensor_{};
    int input_buffer_size_{};
    int out_dim_[4]{};
    std::string context_name_;
    std::string precision_;
};


#endif //HMAP_GENERATOR_HMAP_GENERATOR_H
