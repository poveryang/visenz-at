#include "at_providers.h"

namespace at {

// ---- HeatmapProvider 基类默认实现：无叠加显示、无性能数据 ----

cv::Mat HeatmapProvider::BlendForDisplay(const cv::Mat &image) const
{
    return image;
}

std::string HeatmapProvider::PerfJson() const
{
    return "null";
}

// ---- Null / Simple 占位实现：未配置真实 provider 时的默认行为 ----

HeatmapObservation NullHeatmapProvider::Infer(const FrameContext &)
{
    HeatmapObservation observation;
    observation.available = false;
    observation.source = "null";
    return observation;
}

DecodeFeedback NullDecodeProvider::Decode(const FrameContext &)
{
    DecodeFeedback feedback;
    feedback.attempted = false;
    feedback.summary = "null";
    return feedback;
}

PreprocessReport SimplePreprocessPlugin::Apply(FrameContext &context)
{
    PreprocessReport report;
    if (context.image.empty()) {
        return report;
    }
    report.applied_ops.push_back("pass_through");
    report.quality_delta = 0.0;
    return report;
}

} // namespace at
