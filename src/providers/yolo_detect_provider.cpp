#include "providers/yolo_detect_provider.h"

#include "core/vision/roi_tracker.h"

#include "yolo_detector.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace at {
namespace {

constexpr double kDefaultConfidence = 0.25;
// 观测稳定度的指数平滑系数：新一帧 IoU 占 0.4
constexpr double kStabilityBlend = 0.4;

double ElapsedMs(const std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}

std::string EscapeJson(const std::string &text)
{
    std::ostringstream out;
    for (const char c : text) {
        switch (c) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << c;
                break;
        }
    }
    return out.str();
}

cv::Rect ToPixelRect(const cv::Rect2f &box)
{
    return cv::Rect(static_cast<int>(std::round(box.x)),
                    static_cast<int>(std::round(box.y)),
                    static_cast<int>(std::round(box.width)),
                    static_cast<int>(std::round(box.height)));
}

} // namespace

struct YoloDetectProvider::Impl {
    explicit Impl(YoloDetectConfig input_config)
        : config(std::move(input_config))
    {
        if (config.model_path.empty()) {
            throw std::runtime_error("yolo model path is empty");
        }
        if (config.threshold <= 0.0 || config.threshold > 1.0) {
            config.threshold = kDefaultConfidence;
        }
        const auto start = std::chrono::steady_clock::now();
        detector = std::make_unique<YoloDetector>(config.context, config.precision);
        if (!detector->Init(config.model_path)) {
            throw std::runtime_error("yolo detector init failed: " + config.model_path);
        }
        init_ms = ElapsedMs(start);
    }

    HeatmapObservation Infer(const FrameContext &context)
    {
        const auto total_start = std::chrono::steady_clock::now();
        HeatmapObservation observation;
        observation.source = "yolo-tengine-" + config.context;
        observation.model_version = std::filesystem::path(config.model_path).filename().string();

        last_detections.clear();
        if (context.image.empty()) {
            stability = 0.0;
            previous_roi = {};
            last_observation = observation;
            return observation;
        }

        std::vector<YoloDetection> detections;
        YoloTiming timing;
        const bool ok = detector->Infer(context.image, detections, timing);
        RecordTiming(timing, ElapsedMs(total_start));
        if (!ok) {
            stability = 0.0;
            previous_roi = {};
            last_observation = observation;
            return observation;
        }

        detections.erase(std::remove_if(detections.begin(),
                                        detections.end(),
                                        [this](const YoloDetection &d) {
                                            return d.score < config.threshold;
                                        }),
                         detections.end());
        last_detections = detections;

        if (detections.empty()) {
            stability = 0.0;
            previous_roi = {};
            last_observation = observation;
            return observation;
        }

        const auto best = std::max_element(detections.begin(),
                                           detections.end(),
                                           [](const YoloDetection &a, const YoloDetection &b) {
                                               return a.score < b.score;
                                           });
        const cv::Rect roi = ClipRect(ToPixelRect(best->box), context.image.size());
        if (roi.empty()) {
            stability = 0.0;
            previous_roi = {};
            last_observation = observation;
            return observation;
        }

        double score_sum = 0.0;
        for (const YoloDetection &detection : detections) {
            score_sum += detection.score;
        }

        // 跨帧 IoU 平滑，反映码区位置的时序稳定性，供 AtSession 锁定判据参考
        const double iou = previous_roi.empty() ? 0.0 : RectIoU(previous_roi, roi);
        stability = previous_roi.empty()
                        ? kStabilityBlend
                        : (1.0 - kStabilityBlend) * stability + kStabilityBlend * iou;
        previous_roi = roi;

        observation.available = true;
        observation.roi = roi;
        observation.confidence = std::clamp(static_cast<double>(best->score), 0.0, 1.0);
        observation.feature_strength = score_sum;
        observation.stability = std::clamp(stability, 0.0, 1.0);
        last_observation = observation;
        return observation;
    }

    cv::Mat BlendForDisplay(const cv::Mat &image) const
    {
        if (!config.overlay || image.empty() || last_detections.empty()) {
            return image;
        }

        cv::Mat display;
        if (image.channels() == 1) {
            cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, display, cv::COLOR_BGRA2BGR);
        } else {
            display = image.clone();
        }
        YoloDetector::DrawDetections(display, last_detections);
        if (last_observation.available && !last_observation.roi.empty()) {
            cv::rectangle(display, last_observation.roi, cv::Scalar(0, 255, 0), 2);
        }
        return display;
    }

    std::string TimingJson() const
    {
        const double avg_model_ms = infer_count == 0 ? 0.0 : total_model_ms / infer_count;
        const double avg_total_ms = infer_count == 0 ? 0.0 : total_pipeline_ms / infer_count;
        std::ostringstream out;
        out << "{\"provider\":\"yolo\",\"context\":\"" << EscapeJson(config.context)
            << "\",\"precision\":\"" << EscapeJson(config.precision)
            << "\",\"init_ms\":" << init_ms
            << ",\"infer_count\":" << infer_count
            << ",\"last_model_ms\":" << last_model_ms
            << ",\"avg_model_ms\":" << avg_model_ms
            << ",\"last_total_ms\":" << last_total_ms
            << ",\"avg_total_ms\":" << avg_total_ms << "}";
        return out.str();
    }

    void RecordTiming(const YoloTiming &timing, const double total_ms)
    {
        last_model_ms = timing.inference_ms;
        last_total_ms = total_ms;
        total_model_ms += timing.inference_ms;
        total_pipeline_ms += total_ms;
        ++infer_count;
    }

    YoloDetectConfig config;
    std::unique_ptr<YoloDetector> detector;
    std::vector<YoloDetection> last_detections;
    HeatmapObservation last_observation{};
    cv::Rect previous_roi{};
    double stability = 0.0;
    double init_ms = 0.0;
    double last_model_ms = 0.0;
    double last_total_ms = 0.0;
    double total_model_ms = 0.0;
    double total_pipeline_ms = 0.0;
    int infer_count = 0;
};

YoloDetectProvider::YoloDetectProvider(YoloDetectConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

YoloDetectProvider::~YoloDetectProvider() = default;

HeatmapObservation YoloDetectProvider::Infer(const FrameContext &context)
{
    return impl_->Infer(context);
}

cv::Mat YoloDetectProvider::BlendForDisplay(const cv::Mat &image) const
{
    return impl_->BlendForDisplay(image);
}

std::string YoloDetectProvider::PerfJson() const
{
    return impl_->TimingJson();
}

} // namespace at
