#include "at_heatmap_tengine.h"

#include "hmap_generator.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace at {
namespace {

double ElapsedMs(const std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}

cv::Rect ClipRectToImage(const cv::Rect &rect, const cv::Size &size)
{
    const cv::Rect bounds(0, 0, size.width, size.height);
    return rect & bounds;
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

} // namespace

struct TengineHeatmapProvider::Impl {
    explicit Impl(TengineHeatmapConfig input_config)
        : config(std::move(input_config))
    {
        if (config.model_path.empty()) {
            throw std::runtime_error("heatmap model path is empty");
        }
        const auto start = std::chrono::steady_clock::now();
        generator = std::make_unique<HeatMapGenerator>(config.context, config.precision);
        generator->Init(config.model_path);
        init_ms = ElapsedMs(start);
    }

    HeatmapObservation Infer(const FrameContext &context)
    {
        const auto total_start = std::chrono::steady_clock::now();
        HeatmapObservation observation;
        observation.source = "hmap-tengine-" + config.context;
        observation.model_version = std::filesystem::path(config.model_path).filename().string();
        last_heatmap.release();
        last_observation = observation;

        if (context.image.empty()) {
            return observation;
        }

        cv::Mat gray;
        if (context.image.channels() == 1) {
            gray = context.image;
        } else if (context.image.channels() == 3) {
            cv::cvtColor(context.image, gray, cv::COLOR_BGR2GRAY);
        } else if (context.image.channels() == 4) {
            cv::cvtColor(context.image, gray, cv::COLOR_BGRA2GRAY);
        } else {
            return observation;
        }

        const auto infer_start = std::chrono::steady_clock::now();
        last_heatmap = generator->Infer(gray);
        const double model_ms = ElapsedMs(infer_start);
        const Hotspot hotspot = generator->LocateMaxHotspot(last_heatmap, config.threshold);
        RecordTiming(model_ms, ElapsedMs(total_start));

        if (hotspot.area <= 0 || hotspot.rect.area() <= 0.0 ||
            hotspot.mean_intensity < config.threshold) {
            last_observation = observation;
            return observation;
        }

        const cv::Rect roi(static_cast<int>(std::round(hotspot.rect.x)),
                           static_cast<int>(std::round(hotspot.rect.y)),
                           static_cast<int>(std::round(hotspot.rect.width)),
                           static_cast<int>(std::round(hotspot.rect.height)));
        observation.roi = ClipRectToImage(roi, context.image.size());
        observation.available = !observation.roi.empty();
        observation.confidence = std::clamp(hotspot.mean_intensity / 255.0, 0.0, 1.0);
        observation.feature_strength = hotspot.sum_intensity;
        last_observation = observation;
        return observation;
    }

    cv::Mat BlendForDisplay(const cv::Mat &image) const
    {
        if (!config.overlay || image.empty() || last_heatmap.empty()) {
            return image;
        }

        cv::Mat base;
        if (image.channels() == 1) {
            cv::cvtColor(image, base, cv::COLOR_GRAY2BGR);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, base, cv::COLOR_BGRA2BGR);
        } else {
            base = image.clone();
        }

        cv::Mat heatmap_bgr;
        if (last_heatmap.channels() == 1) {
            cv::applyColorMap(last_heatmap, heatmap_bgr, cv::COLORMAP_JET);
        } else {
            cv::cvtColor(last_heatmap, heatmap_bgr, cv::COLOR_RGB2BGR);
        }
        if (heatmap_bgr.size() != base.size()) {
            cv::resize(heatmap_bgr, heatmap_bgr, base.size(), 0, 0, cv::INTER_NEAREST);
        }

        cv::Mat blended;
        cv::addWeighted(base, 0.55, heatmap_bgr, 0.45, 0.0, blended);
        if (last_observation.available && !last_observation.roi.empty()) {
            cv::rectangle(blended, last_observation.roi, cv::Scalar(0, 255, 0), 2);
        }
        return blended;
    }

    std::string TimingJson() const
    {
        const double avg_model_ms = infer_count == 0 ? 0.0 : total_model_ms / infer_count;
        const double avg_total_ms = infer_count == 0 ? 0.0 : total_pipeline_ms / infer_count;
        std::ostringstream out;
        out << "{\"context\":\"" << EscapeJson(config.context)
            << "\",\"precision\":\"" << EscapeJson(config.precision)
            << "\",\"init_ms\":" << init_ms
            << ",\"infer_count\":" << infer_count
            << ",\"last_model_ms\":" << last_model_ms
            << ",\"avg_model_ms\":" << avg_model_ms
            << ",\"last_total_ms\":" << last_total_ms
            << ",\"avg_total_ms\":" << avg_total_ms << "}";
        return out.str();
    }

    void RecordTiming(const double model_ms, const double total_ms)
    {
        last_model_ms = model_ms;
        last_total_ms = total_ms;
        total_model_ms += model_ms;
        total_pipeline_ms += total_ms;
        ++infer_count;
    }

    TengineHeatmapConfig config;
    std::unique_ptr<HeatMapGenerator> generator;
    cv::Mat last_heatmap;
    HeatmapObservation last_observation{};
    double init_ms = 0.0;
    double last_model_ms = 0.0;
    double last_total_ms = 0.0;
    double total_model_ms = 0.0;
    double total_pipeline_ms = 0.0;
    int infer_count = 0;
};

TengineHeatmapProvider::TengineHeatmapProvider(TengineHeatmapConfig config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
}

TengineHeatmapProvider::~TengineHeatmapProvider() = default;

HeatmapObservation TengineHeatmapProvider::Infer(const FrameContext &context)
{
    return impl_->Infer(context);
}

cv::Mat TengineHeatmapProvider::BlendForDisplay(const cv::Mat &image) const
{
    return impl_->BlendForDisplay(image);
}

std::string TengineHeatmapProvider::PerfJson() const
{
    return impl_->TimingJson();
}

} // namespace at
