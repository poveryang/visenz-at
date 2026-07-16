#include "core/vision/image_quality.h"

#include "core/vision/roi_tracker.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <opencv2/imgproc.hpp>

namespace at {
namespace {

cv::Mat ToGray(const cv::Mat &image)
{
    if (image.empty()) {
        return {};
    }
    if (image.channels() == 1) {
        return image;
    }

    cv::Mat gray;
    if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    return gray;
}

double MeanOfMat(const cv::Mat &mat)
{
    return mat.empty() ? 0.0 : cv::mean(mat)[0];
}

double StdDevOfMat(const cv::Mat &mat)
{
    if (mat.empty()) {
        return 0.0;
    }
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(mat, mean, stddev);
    return stddev[0];
}

} // namespace

ImageQuality AnalyzeImageQuality(const cv::Mat &image, const cv::Rect &roi)
{
    ImageQuality quality;
    if (image.empty()) {
        return quality;
    }

    const cv::Mat gray = ToGray(image);
    const cv::Rect full_image(0, 0, gray.cols, gray.rows);
    const cv::Rect target_roi = roi.empty() ? full_image : ClipRect(roi, gray.size());
    if (target_roi.empty()) {
        return quality;
    }

    const cv::Mat roi_gray = gray(target_roi);
    quality.brightness = MeanOfMat(roi_gray);
    quality.contrast = StdDevOfMat(roi_gray);

    cv::Mat lap;
    cv::Laplacian(roi_gray, lap, CV_32F, 3);
    quality.sharpness = StdDevOfMat(lap);

    cv::Mat saturated;
    cv::threshold(roi_gray, saturated, 250, 255, cv::THRESH_BINARY);
    quality.saturation_ratio = static_cast<double>(cv::countNonZero(saturated)) /
                               std::max(1.0, static_cast<double>(roi_gray.total()));

    cv::Mat blurred;
    cv::GaussianBlur(roi_gray, blurred, cv::Size(3, 3), 0);
    cv::Mat noise;
    cv::absdiff(roi_gray, blurred, noise);
    quality.noise_proxy = MeanOfMat(noise);
    return quality;
}

double GrayEntropy(const cv::Mat &image, const cv::Rect &roi)
{
    if (image.empty()) {
        return 0.0;
    }
    const cv::Mat gray = ToGray(image);
    const cv::Rect full_image(0, 0, gray.cols, gray.rows);
    const cv::Rect target_roi = roi.empty() ? full_image : ClipRect(roi, gray.size());
    if (target_roi.empty()) {
        return 0.0;
    }

    const cv::Mat patch = gray(target_roi);
    std::array<int, 256> histogram{};
    for (int row = 0; row < patch.rows; ++row) {
        const uchar *pixels = patch.ptr<uchar>(row);
        for (int col = 0; col < patch.cols; ++col) {
            ++histogram[pixels[col]];
        }
    }

    const double total = static_cast<double>(patch.total());
    double entropy = 0.0;
    for (const int count : histogram) {
        if (count > 0) {
            const double p = count / total;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

} // namespace at
