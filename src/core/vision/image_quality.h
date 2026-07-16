#ifndef AT_CORE_VISION_IMAGE_QUALITY_H
#define AT_CORE_VISION_IMAGE_QUALITY_H

#include "at_types.h"

namespace at {

ImageQuality AnalyzeImageQuality(const cv::Mat &image, const cv::Rect &roi = {});

// ROI 内灰度直方图熵（bit，[0,8]）。面向解码的信息量指标：
// 均匀过曝/欠曝趋近 0，码区黑白纹理丰富时偏高。
double GrayEntropy(const cv::Mat &image, const cv::Rect &roi = {});

} // namespace at

#endif // AT_CORE_VISION_IMAGE_QUALITY_H
