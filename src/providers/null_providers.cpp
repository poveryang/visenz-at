#include "at_providers.h"

namespace at {

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
