#include "core/vision/roi_tracker.h"

namespace at {

cv::Rect ClipRect(const cv::Rect &rect, const cv::Size &size)
{
    const cv::Rect bounds(0, 0, size.width, size.height);
    const cv::Rect clipped = rect & bounds;
    if (clipped.width <= 0 || clipped.height <= 0) {
        return {};
    }
    return clipped;
}

double RectIoU(const cv::Rect &a, const cv::Rect &b)
{
    if (a.empty() || b.empty()) {
        return 0.0;
    }
    const cv::Rect inter = a & b;
    const double inter_area = static_cast<double>(inter.area());
    const double union_area = static_cast<double>(a.area() + b.area()) - inter_area;
    return union_area > 0.0 ? inter_area / union_area : 0.0;
}

cv::Rect SelectAnalysisRoi(const FrameContext &context)
{
    if (context.image.empty()) {
        return {};
    }

    const cv::Size size(context.image.cols, context.image.rows);
    if (context.heatmap.available) {
        const cv::Rect roi = ClipRect(context.heatmap.roi, size);
        if (!roi.empty()) {
            return roi;
        }
    }
    if (!context.decode.regions.empty()) {
        const cv::Rect roi = ClipRect(context.decode.regions.front(), size);
        if (!roi.empty()) {
            return roi;
        }
    }
    if (!context.manual_roi.empty()) {
        const cv::Rect roi = ClipRect(context.manual_roi, size);
        if (!roi.empty()) {
            return roi;
        }
    }
    return cv::Rect(0, 0, context.image.cols, context.image.rows);
}

} // namespace at
