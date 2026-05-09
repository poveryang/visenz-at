#ifndef AT_AT_SESSION_H
#define AT_AT_SESSION_H

#include "at/at_trace.h"
#include "at/at_types.h"

#include <optional>
#include <vector>

namespace at {

class ATSession {
public:
    explicit ATSession(SessionConfig config);

    void Reset();

    StepResult Step(const FrameInput &input);

    const std::vector<CandidateRecord> &Candidates() const;

private:
    SceneDifficulty ClassifyScene(const ImageQuality &quality,
                                  const HeatmapObservation &heatmap,
                                  const DecodeFeedback &decode) const;
    const StrategyBudget &BudgetFor(SceneDifficulty difficulty) const;
    TuneAction SelectAction(const ImageQuality &quality,
                            const HeatmapObservation &heatmap,
                            const DecodeFeedback &decode,
                            const StrategyBudget &budget) const;
    CameraParams BuildNextParams(const CameraParams &current,
                                 TuneAction action,
                                 const StrategyBudget &budget) const;
    bool ShouldDecode(const ImageQuality &quality,
                      const HeatmapObservation &heatmap,
                      const DecodeFeedback &decode,
                      const StrategyBudget &budget) const;

    SessionConfig config_;
    std::vector<CandidateRecord> candidates_;
    std::optional<CandidateRecord> best_candidate_;
    cv::Rect previous_roi_{};
    int step_index_ = 0;
    int decode_used_ = 0;
    StepPhase phase_ = StepPhase::Observe;
};

} // namespace at

#endif // AT_AT_SESSION_H
