#include "providers/yolo_detect_provider.h"

#include "core/vision/roi_tracker.h"

#include "yolo_detector.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
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

long ReadSelfVmRssKb()
{
    std::ifstream in("/proc/self/status");
    if (!in) {
        return -1;
    }
    std::string key;
    long value = -1;
    std::string unit;
    while (in >> key) {
        if (key == "VmRSS:") {
            in >> value >> unit;
            return value;
        }
        std::string rest;
        std::getline(in, rest);
    }
    return -1;
}

void WriteLoadSnapshot(const YoloDetectConfig &config,
                       bool ready,
                       double init_ms,
                       const std::string &init_error)
{
    namespace fs = std::filesystem;
    fs::path dir;
    if (const char *env = std::getenv("AT_LOG_DIR"); env != nullptr && env[0] != '\0') {
        dir = env;
    } else if (fs::is_directory("/usr/scanner/Log/at")) {
        dir = "/usr/scanner/Log/at";
    } else {
        dir = "/tmp/visenz_at";
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path path = dir / "yolo_load.txt";
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }
    out << "ready=" << (ready ? "true" : "false") << '\n'
        << "lifetime=session" << '\n'
        << "init_ms=" << init_ms << '\n'
        << "vm_rss_kb=" << ReadSelfVmRssKb() << '\n'
        << "model=" << config.model_path << '\n'
        << "context=" << config.context << '\n'
        << "precision=" << config.precision << '\n'
        << "init_error=" << init_error << '\n';
}

// 单次 AT 会话内持有：异步加载，provider 析构时 join 并释放。
struct YoloRuntime {
    explicit YoloRuntime(YoloDetectConfig input_config)
        : config(std::move(input_config))
    {
        if (config.threshold <= 0.0 || config.threshold > 1.0) {
            config.threshold = kDefaultConfidence;
        }
        WriteLoadSnapshot(config, false, 0.0, "session_load_started");
        loader_ = std::thread([this] { LoadDetector(); });
    }

    ~YoloRuntime()
    {
        if (loader_.joinable()) {
            loader_.join();
        }
        WriteLoadSnapshot(config, false, init_ms, "session_released");
    }

    void LoadDetector()
    {
        const auto start = std::chrono::steady_clock::now();
        try {
            auto det = std::make_unique<YoloDetector>(config.context, config.precision);
            if (!det->Init(config.model_path)) {
                std::lock_guard<std::mutex> lock(mutex_);
                init_error_ = "yolo detector init failed: " + config.model_path;
                init_ms = ElapsedMs(start);
                ready_.store(true, std::memory_order_release);
                WriteLoadSnapshot(config, true, init_ms, init_error_);
                return;
            }
            const double ms = ElapsedMs(start);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                detector = std::move(det);
                init_ms = ms;
            }
            ready_.store(true, std::memory_order_release);
            WriteLoadSnapshot(config, true, ms, {});
        } catch (const std::exception &ex) {
            std::lock_guard<std::mutex> lock(mutex_);
            init_error_ = ex.what();
            init_ms = ElapsedMs(start);
            ready_.store(true, std::memory_order_release);
            WriteLoadSnapshot(config, true, init_ms, init_error_);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            init_error_ = "yolo detector init failed: unknown";
            init_ms = ElapsedMs(start);
            ready_.store(true, std::memory_order_release);
            WriteLoadSnapshot(config, true, init_ms, init_error_);
        }
    }

    YoloDetector *TryDetector()
    {
        if (!ready_.load(std::memory_order_acquire)) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return detector.get();
    }

    std::string SnapshotTimingPrefix() const
    {
        const bool ready = ready_.load(std::memory_order_acquire);
        double init_ms_snapshot = 0.0;
        std::string init_error_snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            init_ms_snapshot = init_ms;
            init_error_snapshot = init_error_;
        }
        std::ostringstream out;
        out << "\"provider\":\"yolo\",\"context\":\"" << EscapeJson(config.context)
            << "\",\"precision\":\"" << EscapeJson(config.precision)
            << "\",\"init_deferred\":true"
            << ",\"resident\":false"
            << ",\"lifetime\":\"session\""
            << ",\"init_ready\":" << (ready ? "true" : "false")
            << ",\"init_ms\":" << init_ms_snapshot
            << ",\"init_error\":\"" << EscapeJson(init_error_snapshot) << "\"";
        return out.str();
    }

    YoloDetectConfig config;
    std::unique_ptr<YoloDetector> detector;
    std::thread loader_;
    mutable std::mutex mutex_;
    std::atomic<bool> ready_{false};
    std::string init_error_;
    double init_ms = 0.0;
};

} // namespace

struct YoloDetectProvider::Impl {
    explicit Impl(YoloDetectConfig input_config)
    {
        if (input_config.model_path.empty()) {
            throw std::runtime_error("yolo model path is empty");
        }
        runtime_ = std::make_unique<YoloRuntime>(std::move(input_config));
    }

    // 本帧无有效检测：复位跨帧跟踪状态并发布“不可用”观测。
    HeatmapObservation PublishMiss(const HeatmapObservation &observation)
    {
        stability = 0.0;
        previous_roi = {};
        last_observation = observation;
        return observation;
    }

    HeatmapObservation Infer(const FrameContext &context)
    {
        const auto total_start = std::chrono::steady_clock::now();
        HeatmapObservation observation;
        observation.model_version =
            std::filesystem::path(runtime_->config.model_path).filename().string();

        last_detections.clear();
        if (context.image.empty()) {
            observation.source = "yolo-tengine-" + runtime_->config.context;
            return PublishMiss(observation);
        }

        std::lock_guard<std::mutex> infer_lock(infer_mutex_);
        YoloDetector *det = runtime_->TryDetector();
        if (det == nullptr) {
            if (!runtime_->ready_.load(std::memory_order_acquire)) {
                observation.source = "yolo-tengine-loading";
            } else {
                observation.source = "yolo-tengine-error";
            }
            return PublishMiss(observation);
        }
        observation.source = "yolo-tengine-" + runtime_->config.context;

        std::vector<YoloDetection> detections;
        YoloTiming timing;
        const bool ok = det->Infer(context.image, detections, timing);
        RecordTiming(timing, ElapsedMs(total_start));
        if (!ok) {
            return PublishMiss(observation);
        }

        const double threshold = runtime_->config.threshold;
        detections.erase(std::remove_if(detections.begin(),
                                        detections.end(),
                                        [threshold](const YoloDetection &d) {
                                            return d.score < threshold;
                                        }),
                         detections.end());
        last_detections = detections;

        if (detections.empty()) {
            return PublishMiss(observation);
        }

        const auto best = std::max_element(detections.begin(),
                                           detections.end(),
                                           [](const YoloDetection &a, const YoloDetection &b) {
                                               return a.score < b.score;
                                           });
        const cv::Rect roi = ClipRect(ToPixelRect(best->box), context.image.size());
        if (roi.empty()) {
            return PublishMiss(observation);
        }

        double score_sum = 0.0;
        for (const YoloDetection &detection : detections) {
            score_sum += detection.score;
        }

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
        if (!runtime_->config.overlay || image.empty() || last_detections.empty()) {
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
        out << "{" << runtime_->SnapshotTimingPrefix()
            << ",\"infer_count\":" << infer_count
            << ",\"last_model_ms\":" << last_model_ms
            << ",\"avg_model_ms\":" << avg_model_ms
            << ",\"last_total_ms\":" << last_total_ms
            << ",\"avg_total_ms\":" << avg_total_ms
            << ",\"vm_rss_kb\":" << ReadSelfVmRssKb() << "}";
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

    std::unique_ptr<YoloRuntime> runtime_;
    std::mutex infer_mutex_;
    std::vector<YoloDetection> last_detections;
    HeatmapObservation last_observation{};
    cv::Rect previous_roi{};
    double stability = 0.0;
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
