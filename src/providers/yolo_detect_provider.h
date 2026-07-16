#ifndef AT_SRC_PROVIDERS_YOLO_DETECT_PROVIDER_H
#define AT_SRC_PROVIDERS_YOLO_DETECT_PROVIDER_H

// 私有头：YOLO 检测 provider，仅在库内部使用，不随 include/ 安装。
// 通过既有 HeatmapProvider 接口向 AtSession 提供码区观测，公共接口不变。

#include "at_providers.h"

#include <memory>
#include <string>

namespace at {

struct YoloDetectConfig {
    std::string model_path;
    std::string context = "timvx";
    std::string precision = "uint8";
    // (0,1]: 置信度阈值；>1（heatmap 灰度阈值语义）时回落到默认 0.25
    double threshold = 0.25;
    bool overlay = true;
};

class YoloDetectProvider final : public HeatmapProvider {
public:
    explicit YoloDetectProvider(YoloDetectConfig config);
    ~YoloDetectProvider() override;

    YoloDetectProvider(const YoloDetectProvider &) = delete;
    YoloDetectProvider &operator=(const YoloDetectProvider &) = delete;

    HeatmapObservation Infer(const FrameContext &context) override;

    cv::Mat BlendForDisplay(const cv::Mat &image) const override;
    std::string PerfJson() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace at

#endif // AT_SRC_PROVIDERS_YOLO_DETECT_PROVIDER_H
