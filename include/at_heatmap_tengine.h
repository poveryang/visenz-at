#ifndef AT_AT_HEATMAP_TENGINE_H
#define AT_AT_HEATMAP_TENGINE_H

#include "at_providers.h"

#include <memory>
#include <string>

namespace at {

struct TengineHeatmapConfig {
    std::string model_path;
    std::string context = "timvx";
    std::string precision = "uint8";
    double threshold = kDefaultHeatmapThreshold;
    bool overlay = true;
};

class TengineHeatmapProvider final : public HeatmapProvider {
public:
    explicit TengineHeatmapProvider(TengineHeatmapConfig config);
    ~TengineHeatmapProvider() override;

    TengineHeatmapProvider(const TengineHeatmapProvider &) = delete;
    TengineHeatmapProvider &operator=(const TengineHeatmapProvider &) = delete;

    HeatmapObservation Infer(const FrameContext &context) override;

    cv::Mat BlendForDisplay(const cv::Mat &image) const override;
    std::string PerfJson() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at

#endif // AT_AT_HEATMAP_TENGINE_H
