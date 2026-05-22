#include "at_orchestrator.h"

#if defined(AT_WITH_HMAP_TENGINE)
#include "at_heatmap_tengine.h"
#endif

#include <filesystem>
#include <stdexcept>

namespace at {
namespace {

std::unique_ptr<HeatmapProvider> MakeHeatmapProvider(const HeatmapConfig &config)
{
    if (config.model_path.empty()) {
        return std::make_unique<NullHeatmapProvider>();
    }
    if (!std::filesystem::exists(config.model_path)) {
        throw std::runtime_error("heatmap model not found: " + config.model_path);
    }

#if defined(AT_WITH_HMAP_TENGINE)
    TengineHeatmapConfig tconfig;
    tconfig.model_path = config.model_path;
    tconfig.context = config.context;
    tconfig.precision = config.precision;
    tconfig.threshold = config.threshold;
    tconfig.overlay = config.overlay;
    return std::make_unique<TengineHeatmapProvider>(std::move(tconfig));
#else
    throw std::runtime_error("heatmap model requires AT built with HMAP support");
#endif
}

} // namespace

AtOrchestrator::AtOrchestrator(SessionConfig config,
                               std::unique_ptr<DecodeProvider> decode,
                               std::unique_ptr<PreprocessPlugin> preprocess)
    : session_(config),
      heatmap_(MakeHeatmapProvider(config.heatmap)),
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

cv::Mat AtOrchestrator::BlendForDisplay(const cv::Mat &image) const
{
    return heatmap_->BlendForDisplay(image);
}

std::string AtOrchestrator::HeatmapPerfJson() const
{
    return heatmap_->PerfJson();
}

} // namespace at
