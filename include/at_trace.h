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
    int trace_version = kTraceVersion;
    int step_index = 0;
    int stage_step_count = 0;
    StepPhase phase = StepPhase::Observe;
    TuneAction action = TuneAction::Hold;
    FinishReason finish_reason = FinishReason::None;
    ImageQuality quality{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
    PreprocessReport preprocess{};
    int decode_used = 0;
    int decode_budget = 0;
    int active_max_steps = 0;
    std::string reason;
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
