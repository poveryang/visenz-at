#include "core/candidate_store.h"

#include "core/vision/image_quality.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace at {
namespace {

// ---- 面向解码的候选帧评分 ----
// 解码成功绝对支配；未解码时以“码区可读性”代理指标排序：
// 清晰度（Laplacian σ）、对比度（灰度 σ）、灰度熵为主，亮度只要求落在
// 就绪窗口内（窗外线性衰减），饱和（高光溢出）按占比惩罚，
// 有检测观测的帧再按置信度加成，使锁定 ROI 的帧优先胜出。
constexpr double kDecodeSuccessScore = 2.0;   // 解码成功的支配性得分
constexpr double kDecodePartialWeight = 0.5;  // 解码失败但有部分得分时的权重
constexpr double kSharpnessWeight = 0.30;
constexpr double kSharpnessFullScale = 80.0;  // Laplacian σ 满量程（达到即 1 分）
constexpr double kContrastWeight = 0.20;
constexpr double kContrastFullScale = 64.0;   // 灰度 σ 满量程
constexpr double kEntropyWeight = 0.15;
constexpr double kEntropyFullScale = 8.0;     // 8bit 灰度熵上限
constexpr double kBrightnessWeight = 0.15;
constexpr double kHeatmapWeight = 0.20;
constexpr double kSaturationPenalty = 0.25;
constexpr double kSaturationPenaltyGain = 4.0; // 饱和占比放大（25% 饱和即满惩罚）

double Clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

// 就绪窗口内 1 分；窗外按半窗宽线性衰减到 0。
double BrightnessWindowScore(double brightness, const FlowConfig &flow)
{
    if (brightness >= flow.min_ready_brightness && brightness <= flow.max_ready_brightness) {
        return 1.0;
    }
    const double window = std::max(1.0, flow.max_ready_brightness - flow.min_ready_brightness);
    const double distance = brightness < flow.min_ready_brightness
                                ? flow.min_ready_brightness - brightness
                                : brightness - flow.max_ready_brightness;
    return std::max(0.0, 1.0 - distance / (window * 0.5));
}

double ScoreCandidate(const FrameContext &context,
                      const ImageQuality &quality,
                      double entropy,
                      const FlowConfig &flow)
{
    const double decode_score = context.decode.success
                                    ? kDecodeSuccessScore
                                    : kDecodePartialWeight * Clamp01(context.decode.score);
    const double sharpness_score = Clamp01(quality.sharpness / kSharpnessFullScale);
    const double contrast_score = Clamp01(quality.contrast / kContrastFullScale);
    const double entropy_score = Clamp01(entropy / kEntropyFullScale);
    const double brightness_score = BrightnessWindowScore(quality.brightness, flow);
    const double heatmap_score = context.heatmap.available ? Clamp01(context.heatmap.confidence) : 0.0;
    const double saturation_over = Clamp01(quality.saturation_ratio * kSaturationPenaltyGain);

    return decode_score +
           sharpness_score * kSharpnessWeight +
           contrast_score * kContrastWeight +
           entropy_score * kEntropyWeight +
           brightness_score * kBrightnessWeight +
           heatmap_score * kHeatmapWeight -
           saturation_over * kSaturationPenalty;
}

} // namespace

CandidateRecord MakeCandidate(const FrameContext &context,
                              const cv::Rect &roi,
                              const ImageQuality &quality,
                              StepPhase phase,
                              TuneAction action,
                              const FlowConfig &flow)
{
    CandidateRecord candidate;
    candidate.params = context.current_params;
    candidate.roi = roi;
    candidate.phase = phase;
    candidate.action = action;
    candidate.quality = quality;
    candidate.heatmap = context.heatmap;
    candidate.decode = context.decode;

    const double entropy = GrayEntropy(context.image, roi);
    candidate.score = ScoreCandidate(context, quality, entropy, flow);

    std::ostringstream reason;
    reason << "phase=" << ToString(phase)
           << " action=" << ToString(action)
           << " brightness=" << quality.brightness
           << " sharpness=" << quality.sharpness
           << " contrast=" << quality.contrast
           << " entropy=" << entropy
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
