#ifndef AT_CORE_VISION_ROI_TRACKER_H
#define AT_CORE_VISION_ROI_TRACKER_H

#include "at_types.h"

namespace at {

cv::Rect ClipRect(const cv::Rect &rect, const cv::Size &size);
double RectIoU(const cv::Rect &a, const cv::Rect &b);
cv::Rect SelectAnalysisRoi(const FrameContext &context);

} // namespace at

#endif // AT_CORE_VISION_ROI_TRACKER_H
