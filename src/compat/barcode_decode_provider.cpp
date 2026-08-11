#include "compat/barcode_decode_provider.h"

namespace at {
namespace compat {

BarcodeDecodeProvider::BarcodeDecodeProvider(BarcodeWrapperBase *wrapper) : wrapper_(wrapper) {}

DecodeFeedback BarcodeDecodeProvider::Decode(const FrameContext &context)
{
    DecodeFeedback feedback;
    feedback.attempted = true;
    if (wrapper_ == nullptr || context.image.empty()) {
        feedback.success = false;
        feedback.summary = "barcode wrapper unavailable";
        return feedback;
    }

    last_ar_info_.reset();
    const cv::Rect roi = context.manual_roi.area() > 0
                             ? context.manual_roi
                             : (context.heatmap.available ? context.heatmap.roi : cv::Rect{});
    try {
        feedback.regions = wrapper_->Decode(context.image, last_ar_info_, roi);
        feedback.success = !feedback.regions.empty();
        feedback.score = feedback.success ? 1.0 : 0.0;
        feedback.summary = feedback.success ? "barcode decode success" : "barcode decode miss";
    } catch (...) {
        feedback.success = false;
        feedback.summary = "barcode decode exception";
    }
    return feedback;
}

}  // namespace compat
}  // namespace at
