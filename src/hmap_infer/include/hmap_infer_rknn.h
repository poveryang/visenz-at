#ifndef HMAP_INFER_RK_H
#define HMAP_INFER_RK_H

#include "hmap_infer_base.h"
#include "rknn_api.h"

class HMapInferRK : public HMapInferBase {
public:
    HMapInferRK() = default;

    ~HMapInferRK();

    void Init(const std::string &model_path) final;

    cv::Mat Inference(const cv::Mat &image) final;

    cv::Mat PreProcess(const cv::Mat &image) final;

    cv::Mat PostProcess(cv::Mat &image) override;


private:
    // Model context
    rknn_context rknn_context_{};
    unsigned char* model_ = nullptr;
    rknn_input inputs_[1]{};
};

#endif //HMAP_INFER_RK_H