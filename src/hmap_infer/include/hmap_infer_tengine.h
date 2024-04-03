#ifndef HMAP_INFER_TENGINE_H
#define HMAP_INFER_TENGINE_H

#include "hmap_infer_base.h"
#include "c_api.h"
#include <opencv2/opencv.hpp>


class HMapInferTengine : public HMapInferBase {
public:
    HMapInferTengine() = default;

    ~HMapInferTengine();

    void Init(const std::string &model_path) final;

    cv::Mat Inference(const cv::Mat &image) final;

    cv::Mat PreProcess(const cv::Mat &image) final;

    cv::Mat PostProcess(cv::Mat &image) override;

    static cv::Mat Sigmoid(const cv::Mat &image);

private:
    // quantization parameters
    float input_scale = 0.f;
    int input_zero_point = 0;
    float output_scale = 0.f;
    int output_zero_point = 0;

    // Model context
    graph_t graph_{};
    options opt_{};
    tensor_t input_tensor_{};
    tensor_t output_tensor_{};
    int input_buffer_size_{};
    int out_dim_[4]{};
    std::string context_name_ = "timvx";
    std::string precision_ = "uint8";
};

#endif //HMAP_INFER_TENGINE_H