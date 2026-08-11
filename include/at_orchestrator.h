#ifndef AT_AT_ORCHESTRATOR_H
#define AT_AT_ORCHESTRATOR_H

#include "at_providers.h"
#include "at_run_log.h"
#include "at_session.h"

#include <memory>

namespace at {

class AtOrchestrator {
public:
    explicit AtOrchestrator(SessionConfig config,
                            std::unique_ptr<DecodeProvider> decode = nullptr,
                            std::unique_ptr<PreprocessPlugin> preprocess = nullptr);
    ~AtOrchestrator();

    void Reset();

    // 中断未完成的 run：写 JSONL end(Interrupted)，并释放 heatmap。
    void Abort();

    // AT 结束后释放 YOLO 等 heatmap 资源（不跨会话常驻）。
    void ReleaseHeatmap();

    StepResult ProcessStep(FrameContext context);
    HeatmapObservation ObserveHeatmap(FrameContext context);

    cv::Mat BlendForDisplay(const cv::Mat &image) const;
    std::string HeatmapPerfJson() const;

    const AtRunLog &run_log() const { return run_log_; }

private:
    SessionConfig config_;
    AtSession session_;
    AtRunLog run_log_;
    std::unique_ptr<HeatmapProvider> heatmap_;
    std::unique_ptr<DecodeProvider> decode_;
    std::unique_ptr<PreprocessPlugin> preprocess_;
};

} // namespace at

#endif // AT_AT_ORCHESTRATOR_H
