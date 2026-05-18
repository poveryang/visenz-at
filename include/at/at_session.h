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
    Observation BuildObservation(const FrameInput &input) const;
    void UpdateDifficultyState(SceneDifficulty difficulty);
    const StrategyBudget &ActiveBudget() const;
    TuningDecision Decide(const Observation &observation,
                          const StrategyBudget &budget) const;
    StepPhase SelectConfiguredPhase(const Observation &observation,
                                    const StrategyBudget &budget) const;
    TuneAction SelectActionForPhase(StepPhase phase,
                                    const Observation &observation) const;
    bool IsStageEnabled(StepPhase phase) const;
    bool StageHasRemainingSteps(StepPhase phase, int max_steps) const;
    bool NeedsBrightnessPrecondition(const ImageQuality &quality) const;
    bool SameParams(const CameraParams &lhs, const CameraParams &rhs) const;
    void UpdateStageState(StepPhase phase);
    SceneDifficulty ClassifyScene(const ImageQuality &quality,
                                  const HeatmapObservation &heatmap,
                                  const DecodeFeedback &decode) const;
    const StrategyBudget &BudgetFor(SceneDifficulty difficulty) const;
    CameraParams BuildNextParams(const CameraParams &current,
                                 TuneAction action,
                                 const ImageQuality &quality,
                                 const StrategyBudget &budget) const;
    CameraParams BuildBrightnessPreconditionParams(const CameraParams &current,
                                                   TuneAction action,
                                                   const ImageQuality &quality,
                                                   const StrategyBudget &budget) const;
    bool ShouldDecode(const ImageQuality &quality,
                      const HeatmapObservation &heatmap,
                      const DecodeFeedback &decode,
                      const StrategyBudget &budget) const;
    void RecordExposureSample(const Observation &observation);

    SessionConfig config_;
    std::vector<CandidateRecord> candidates_;
    std::optional<CandidateRecord> best_candidate_;
    cv::Rect previous_roi_{};
    SessionState state_{};

    struct ExposureSample {
        CameraParams params{};
        double brightness = 0.0;
        double saturation_ratio = 0.0;
    };
    std::vector<ExposureSample> exposure_samples_;
};

} // namespace at

#endif // AT_AT_SESSION_H
