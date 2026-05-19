#include "core/candidate_pool.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace at {
namespace {

double Clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

CandidateRecord MakeCandidate(const FrameInput &input,
                              const cv::Rect &roi,
                              const ImageQuality &quality,
                              SceneDifficulty difficulty,
                              StepPhase phase,
                              TuneAction action)
{
    CandidateRecord candidate;
    candidate.params = input.current_params;
    candidate.roi = roi;
    candidate.difficulty = difficulty;
    candidate.phase = phase;
    candidate.action = action;
    candidate.quality = quality;
    candidate.heatmap = input.heatmap;
    candidate.decode = input.decode;

    const double decode_score = input.decode.success ? 1.0 : 0.0;
    const double brightness_score = 1.0 - Clamp01(std::abs(quality.brightness - 110.0) / 110.0);
    const double saturation_score = 1.0 - Clamp01(quality.saturation_ratio * 4.0);
    const double noise_score = 1.0 - Clamp01(quality.noise_proxy / 30.0);
    const double sharpness_score = Clamp01(quality.sharpness / 80.0);
    const double heatmap_score = input.heatmap.available ? input.heatmap.confidence : 0.0;

    candidate.score = decode_score +
                      brightness_score * 0.35 +
                      saturation_score * 0.25 +
                      noise_score * 0.15 +
                      sharpness_score * 0.15 +
                      heatmap_score * 0.30;

    std::ostringstream reason;
    reason << ToString(difficulty)
           << " action=" << ToString(action)
           << " brightness=" << quality.brightness
           << " contrast=" << quality.contrast
           << " saturation=" << quality.saturation_ratio
           << " decode=" << (input.decode.success ? "success" : "none/fail");
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
