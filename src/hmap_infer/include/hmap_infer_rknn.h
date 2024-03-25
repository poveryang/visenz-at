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

    void SetSrcSize(const cv::Size &size) {
        src_size_ = size;
    }

    void SetDstSize(const cv::Size &size) {
        dst_size_ = size;
    }

private:
    // Size of input and output
    cv::Size src_size_ = cv::Size(640, 400);
    cv::Size dst_size_ = cv::Size(640, 400);

    // Model context
    rknn_context rknn_context_{};
    unsigned char* model_ = nullptr;
    rknn_input inputs_[1]{};
};

#endif //HMAP_INFER_RK_H