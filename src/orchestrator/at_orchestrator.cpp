#include "at_orchestrator.h"

#if defined(AT_WITH_YOLO_TENGINE)
#include "providers/yolo_detect_provider.h"
#endif

#include <filesystem>
#include <stdexcept>

namespace at {
namespace {

// 码区观测 provider：model_path 非空即加载 YOLO 检测模型。
// 复用既有 HeatmapConfig/HeatmapProvider 契约，公共接口无需改动。
std::unique_ptr<HeatmapProvider> MakeHeatmapProvider(const HeatmapConfig &config)
{
    if (config.model_path.empty()) {
        return std::make_unique<NullHeatmapProvider>();
    }
    if (!std::filesystem::exists(config.model_path)) {
        throw std::runtime_error("detect model not found: " + config.model_path);
    }

#if defined(AT_WITH_YOLO_TENGINE)
    YoloDetectConfig yconfig;
    yconfig.model_path = config.model_path;
    yconfig.context = config.context;
    yconfig.precision = config.precision;
    yconfig.threshold = config.threshold;
    yconfig.overlay = config.overlay;
    return std::make_unique<YoloDetectProvider>(std::move(yconfig));
#else
    throw std::runtime_error("detect model requires AT built with YOLO support");
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

HeatmapObservation AtOrchestrator::ObserveHeatmap(FrameContext context)
{
    context.preprocess = preprocess_->Apply(context);
    if (context.heatmap.available) {
        return context.heatmap;
    }
    return heatmap_->Infer(context);
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
