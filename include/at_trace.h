#ifndef AT_AT_TRACE_H
#define AT_AT_TRACE_H

#include "at_types.h"

#include <optional>
#include <string>
#include <vector>

namespace at {

struct CandidateRecord {
    CameraParams params{};
    cv::Rect roi{};
    StepPhase phase = StepPhase::Observe;
    TuneAction action = TuneAction::Hold;
    ImageQuality quality{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
    double score = 0.0;
    std::string reason;
};

struct StepTrace {
    int step_index = 0;
    int stage_step_count = 0;
    StepPhase phase = StepPhase::Observe;
    TuneAction action = TuneAction::Hold;
    FinishReason finish_reason = FinishReason::None;
    ImageQuality quality{};
    HeatmapObservation heatmap{};
    std::string reason;
    int decode_used = 0;
};

struct StepResult {
    bool finished = false;
    bool need_decode = false;
    CameraParams next_params{};
    std::optional<CandidateRecord> best_candidate;
    StepTrace trace{};
};

} // namespace at

#endif // AT_AT_TRACE_H
