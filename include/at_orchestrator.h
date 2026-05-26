#ifndef AT_AT_ORCHESTRATOR_H
#define AT_AT_ORCHESTRATOR_H

#include "at_providers.h"
#include "at_session.h"

#include <memory>

namespace at {

class AtOrchestrator {
public:
    explicit AtOrchestrator(SessionConfig config,
                            std::unique_ptr<DecodeProvider> decode = nullptr,
                            std::unique_ptr<PreprocessPlugin> preprocess = nullptr);

    void Reset();

    StepResult ProcessStep(FrameContext context);
    HeatmapObservation ObserveHeatmap(FrameContext context);

    cv::Mat BlendForDisplay(const cv::Mat &image) const;
    std::string HeatmapPerfJson() const;

private:
    AtSession session_;
    std::unique_ptr<HeatmapProvider> heatmap_;
    std::unique_ptr<DecodeProvider> decode_;
    std::unique_ptr<PreprocessPlugin> preprocess_;
};

} // namespace at

#endif // AT_AT_ORCHESTRATOR_H
