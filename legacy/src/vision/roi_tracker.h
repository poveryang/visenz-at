#ifndef AT_VISION_ROI_TRACKER_H
#define AT_VISION_ROI_TRACKER_H

#include "at/at_types.h"

namespace at {

cv::Rect ClipRect(const cv::Rect &rect, const cv::Size &size);

double RectIoU(const cv::Rect &a, const cv::Rect &b);

cv::Rect SelectAnalysisRoi(const FrameInput &input);

} // namespace at

#endif // AT_VISION_ROI_TRACKER_H
