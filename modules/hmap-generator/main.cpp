#include <iostream>
#include "hmap_generator.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>


cv::Mat preprocess(const cv::Mat &image, float mean, float var, float scale, int zero_point) {
    cv::Mat dst_img;
    float tmp = var * scale;
    cv::resize(image, dst_img, cv::Size(640, 400));
    dst_img.convertTo(dst_img, CV_32FC1, 1.0 / 255);
    dst_img = (dst_img - cv::Scalar(mean)) / cv::Scalar(tmp) + cv::Scalar(zero_point);
    dst_img.convertTo(dst_img, CV_8UC1);

    cv::resize(image, dst_img, cv::Size(640, 400));
    dst_img = dst_img * scale + zero_point;
    return dst_img;
}


cv::Mat postprocess(const cv::Mat &image, float scale, int zero_point) {
    cv::Mat dst_img;
    image.convertTo(dst_img, CV_32FC3);
    dst_img = (dst_img - zero_point) * scale;
    dst_img.convertTo(dst_img, CV_8UC3, 255);
    cv::resize(dst_img, dst_img, cv::Size(1280, 800));
    cv::resize(image, dst_img, cv::Size(1280, 800), 0, 0, cv::INTER_NEAREST);
    return dst_img;
}


cv::Mat preprocess_uint8(const cv::Mat &image, float scale, int zero_point) {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(640, 400));
    dst_img = dst_img * scale + zero_point;
    return dst_img;
}


cv::Mat preprocess_fp32(const cv::Mat &image) {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(640, 400));
    dst_img.convertTo(dst_img, CV_32FC1, 1.0 / 255);
    dst_img = (dst_img - cv::Scalar(0.4329))/ cv::Scalar(0.2349);
    return dst_img;
}


cv::Mat postprocess_uint8(const cv::Mat &image) {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(1280, 800), 0, 0, cv::INTER_NEAREST);
    return dst_img;
}


cv::Mat postprocess_fp32(const cv::Mat &image) {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(1280, 800), 0, 0, cv::INTER_NEAREST);
    dst_img.convertTo(dst_img, CV_8UC3, 255);
    return dst_img;
}


int main(int argc, char **argv) {
    std::string precision = argv[1];
    std::string model_path = argv[2];
    std::string image_path = argv[3];

    HeatMapGenerator hmap_generator = HeatMapGenerator("cpu", precision);
    hmap_generator.Init(model_path);
    cv::Mat src_img = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
    cv::Mat in_img;
    // calculate the time of inference
    int64 start = cv::getTickCount();
    if (precision == "fp32") {
        in_img = preprocess_fp32(src_img);
    } else if (precision == "uint8") {
        float input_scale = 0.2349;
        int input_zero_point = int(0.4329 * 255);
        in_img = preprocess_uint8(src_img, input_scale, input_zero_point);
    }
    int64 end = cv::getTickCount();
    double preprocess_time = double(end - start) / cv::getTickFrequency();
    fprintf(stdout, "Preprocess time: %f ms\n", preprocess_time * 1000);

    start = cv::getTickCount();
    cv::Mat heatmap = hmap_generator.Infer(in_img);
    end = cv::getTickCount();
    double inference_time = double(end - start) / cv::getTickFrequency();
    fprintf(stdout, "Inference time: %f ms\n", inference_time * 1000);

    cv::Mat heatmap_post;
    start = cv::getTickCount();
    if (precision == "fp32") {
        heatmap_post = postprocess_fp32(heatmap);
    } else if (precision == "uint8") {
        heatmap_post = postprocess_uint8(heatmap);
    }
    end = cv::getTickCount();
    double time = double(end - start) / cv::getTickFrequency();
    fprintf(stdout, "Postprocess time: %f ms\n", time * 1000);

    cv::imwrite("heatmap.png", heatmap_post);

    cv::Mat blend_img;
    cv::cvtColor(src_img, src_img, cv::COLOR_GRAY2BGR);
    cv::cvtColor(heatmap_post, heatmap_post, cv::COLOR_RGB2BGR);
    cv::addWeighted(src_img, 0.5, heatmap_post, 0.5, 0, blend_img);
    cv::imshow("result", blend_img);
    cv::waitKey(0);

    return 0;
}

