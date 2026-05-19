#ifndef AT_VISION_IMAGE_QUALITY_H
#define AT_VISION_IMAGE_QUALITY_H

#include "at/at_types.h"

namespace at {

ImageQuality AnalyzeImageQuality(const cv::Mat &image, const cv::Rect &roi = {});

} // namespace at

#endif // AT_VISION_IMAGE_QUALITY_H
