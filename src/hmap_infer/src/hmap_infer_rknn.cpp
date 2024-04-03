#ifdef USE_RKNN

#include <fstream>
#include <opencv2/core/core.hpp>

#include "hmap_infer_rknn.h"

HMapInferRK::~HMapInferRK() {
    // Release RKNN context and model
    if (rknn_context_ != 0) {
        rknn_destroy(rknn_context_);
    }

    if (model_ != nullptr) {
        free(model_);
    }
}

void HMapInferRK::Init(const std::string &model_path) {
    // Load RKNN model
    std::ifstream file(model_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open " << model_path << '\n';
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    model_ = (unsigned char*)malloc(size);
    if (!file.read((char*)model_, size)) {
        std::cerr << "Failed to read " << model_path << '\n';
        return;
    }

    // Init RKNN
    int ret = rknn_init(&rknn_context_, model_, size, 0, nullptr);
    if (ret != RKNN_SUCC) {
        printf("rknn_init fail! ret=%d\n", ret);
        exit(ret);
    }

    // TODO: set core mask temporarily
    rknn_set_core_mask(rknn_context_, RKNN_NPU_CORE_0);

    // Get model's io info
    rknn_input_output_num io_num;
    ret = rknn_query(rknn_context_, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        exit(ret);
    } else {
        printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);
    }

    // Get model's input attribute
    rknn_tensor_attr input_attrs[1];
    memset(input_attrs, 0, sizeof(input_attrs));
    input_attrs[0].index = 0;
    ret = rknn_query(rknn_context_, RKNN_QUERY_INPUT_ATTR, &(input_attrs[0]), sizeof(rknn_tensor_attr));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        exit(ret);
    }

    // Get model's output attribute
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    output_attrs[0].index = 0;
    ret = rknn_query(rknn_context_, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[0]), sizeof(rknn_tensor_attr));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        exit(ret);
    }

    // Initialize input
    memset(inputs_, 0, sizeof(inputs_));
    inputs_[0].index = 0;
    inputs_[0].type  = RKNN_TENSOR_INT8;
    inputs_[0].size  = this->infer_size_wh_.width * this->infer_size_wh_.height * 1 * sizeof(uint8_t);
    inputs_[0].fmt   = RKNN_TENSOR_NHWC;
}

cv::Mat HMapInferRK::Inference(const cv::Mat &image) {
    // Preprocess
    cv::Mat input_image = PreProcess(image);

    // Set input data
    inputs_[0].buf = input_image.data;
    int ret = rknn_inputs_set(rknn_context_, 1, inputs_);
    if (ret != RKNN_SUCC) {
        printf("rknn_input_set fail! ret=%d\n", ret);
        exit(ret);
    } else {
        printf("rknn_input_set success!\n");
    }

    // Run model
    ret = rknn_run(rknn_context_, nullptr);
    if (ret != RKNN_SUCC) {
        printf("rknn_run fail! ret=%d\n", ret);
        exit(ret);
    } else {
        printf("rknn_run success!\n");
    }

    // Get output
    rknn_output outputs[1];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].want_float = 1;
    ret = rknn_outputs_get(rknn_context_, 1, outputs, nullptr);
    if (ret != RKNN_SUCC) {
        printf("rknn_outputs_get fail! ret=%d\n", ret);
        exit(ret);
    } else {
        printf("rknn_outputs_get success!\n");
    }
    cv::Mat tempBuffer(3, dst_size_.width * dst_size_.height, CV_32FC1, outputs[0].buf);

    // Post process
    cv::Mat heatmap = PostProcess(tempBuffer);

    return heatmap;
}

cv::Mat HMapInferRK::PreProcess(const cv::Mat &image) {
    // Resize image
    printf("Resize %d %d to %d %d\n", image.cols, image.rows, this->infer_size_wh_.width, this->infer_size_wh_.height);
    cv::Mat resized_img = image;
    cv::resize(resized_img, resized_img, this->infer_size_wh_, 0, 0, cv::INTER_LINEAR);
    resized_img = resized_img.reshape((this->infer_size_wh_.height, this->infer_size_wh_.width, 1));
    return resized_img;
}

cv::Mat HMapInferRK::PostProcess(cv::Mat &chw_buffer) {
    // Reshape tempBuffer(4, h * w) to (h, w, 4)
    std::vector<cv::Mat> chs(4);
    for (int i = 0; i < 4; ++i)
    {
        cv::Mat ch_buffer = chw_buffer.row(i);
        cv::exp(ch_buffer, ch_buffer);
        ch_buffer = ch_buffer / (ch_buffer + 1.0f);
        chs[i] = ch_buffer.reshape(0, dst_size_.height);
    }

    cv::Mat hwc_img;
    cv::merge(chs, hwc_img);

    // TODO: clip min value temporarily
    cv::Mat mask = hwc_img < this->hmap_intensity_thre_;
    hwc_img.setTo(0, mask);
    hwc_img.convertTo(hwc_img, CV_8UC4, 255);

    return hwc_img;
}

#endif //USE_RKNN