#include "core/candidate_store.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace at {
namespace {

// ---- 候选帧评分模型 ----
// score = decode + Σ(归一化质量分 × 权重)。decode 成功恒为 1.0，压过其余分项。
constexpr double kIdealBrightness = 110.0;        // 灰度均值最优点，偏离线性扣分
constexpr double kSaturationPenaltyGain = 4.0;    // 饱和占比放大系数（25% 饱和即 0 分）
constexpr double kNoiseFullScale = 30.0;          // 噪声代理满量程（达到即 0 分）
constexpr double kSharpnessFullScale = 80.0;      // 清晰度满量程（达到即 1 分）
constexpr double kBrightnessWeight = 0.35;
constexpr double kSaturationWeight = 0.25;
constexpr double kNoiseWeight = 0.15;
constexpr double kSharpnessWeight = 0.15;
constexpr double kHeatmapWeight = 0.30;

double Clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double ScoreCandidate(const FrameContext &context, const ImageQuality &quality)
{
    const double decode_score = context.decode.success ? 1.0 : context.decode.score;
    const double brightness_score =
        1.0 - Clamp01(std::abs(quality.brightness - kIdealBrightness) / kIdealBrightness);
    const double saturation_score = 1.0 - Clamp01(quality.saturation_ratio * kSaturationPenaltyGain);
    const double noise_score = 1.0 - Clamp01(quality.noise_proxy / kNoiseFullScale);
    const double sharpness_score = Clamp01(quality.sharpness / kSharpnessFullScale);
    const double heatmap_score = context.heatmap.available ? context.heatmap.confidence : 0.0;

    return decode_score +
           brightness_score * kBrightnessWeight +
           saturation_score * kSaturationWeight +
           noise_score * kNoiseWeight +
           sharpness_score * kSharpnessWeight +
           heatmap_score * kHeatmapWeight;
}

} // namespace

CandidateRecord MakeCandidate(const FrameContext &context,
                              const cv::Rect &roi,
                              const ImageQuality &quality,
                              StepPhase phase,
                              TuneAction action)
{
    CandidateRecord candidate;
    candidate.params = context.current_params;
    candidate.roi = roi;
    candidate.phase = phase;
    candidate.action = action;
    candidate.quality = quality;
    candidate.heatmap = context.heatmap;
    candidate.decode = context.decode;
    candidate.score = ScoreCandidate(context, quality);

    std::ostringstream reason;
    reason << "phase=" << ToString(phase)
           << " action=" << ToString(action)
           << " brightness=" << quality.brightness
           << " sharpness=" << quality.sharpness
           << " decode=" << (context.decode.success ? "success" : "none/fail");
    candidate.reason = reason.str();
    return candidate;
}

void UpdateBestCandidate(const CandidateRecord &candidate,
                         std::optional<CandidateRecord> &best_candidate)
{
    if (!best_candidate || candidate.score > best_candidate->score) {
        best_candidate = candidate;
    }
}

} // namespace at
