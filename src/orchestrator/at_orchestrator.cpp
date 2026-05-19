#include "at_orchestrator.h"

namespace at {

AtOrchestrator::AtOrchestrator(SessionConfig config,
                                 std::unique_ptr<HeatmapProvider> heatmap,
                                 std::unique_ptr<DecodeProvider> decode,
                                 std::unique_ptr<PreprocessPlugin> preprocess)
    : session_(std::move(config)),
      heatmap_(heatmap ? std::move(heatmap) : std::make_unique<NullHeatmapProvider>()),
      decode_(decode ? std::move(decode) : std::make_unique<NullDecodeProvider>()),
      preprocess_(preprocess ? std::move(preprocess) : std::make_unique<SimplePreprocessPlugin>())
{
}

void AtOrchestrator::Reset()
{
    session_.Reset();
}

StepResult AtOrchestrator::ProcessStep(FrameContext context)
{
    context.preprocess = preprocess_->Apply(context);
    if (!context.heatmap.available) {
        context.heatmap = heatmap_->Infer(context);
    }

    StepResult result = session_.ProcessStep(context);

    if (result.need_decode && !context.decode.attempted) {
        context.decode = decode_->Decode(context);
        result = session_.ProcessStep(context);
    }

    return result;
}

} // namespace at
