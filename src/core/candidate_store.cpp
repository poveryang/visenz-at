#include "core/candidate_store.h"

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

    const double decode_score = context.decode.success ? 1.0 : context.decode.score;
    const double brightness_score = 1.0 - Clamp01(std::abs(quality.brightness - 110.0) / 110.0);
    const double saturation_score = 1.0 - Clamp01(quality.saturation_ratio * 4.0);
    const double noise_score = 1.0 - Clamp01(quality.noise_proxy / 30.0);
    const double sharpness_score = Clamp01(quality.sharpness / 80.0);
    const double heatmap_score = context.heatmap.available ? context.heatmap.confidence : 0.0;

    candidate.score = decode_score +
                      brightness_score * 0.35 +
                      saturation_score * 0.25 +
                      noise_score * 0.15 +
                      sharpness_score * 0.15 +
                      heatmap_score * 0.30;

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
