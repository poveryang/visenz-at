#ifndef AT_AT_SESSION_H
#define AT_AT_SESSION_H

#include "at_trace.h"
#include "at_types.h"

#include <optional>
#include <vector>

namespace at {

class AtSession {
public:
    explicit AtSession(SessionConfig config);

    void Reset();

    StepResult ProcessStep(const FrameContext &context);

    const std::vector<CandidateRecord> &Candidates() const;

private:
    Observation BuildObservation(const FrameContext &context) const;
    StepDecision Decide(const Observation &observation);
    bool SameParams(const CameraParams &lhs, const CameraParams &rhs) const;
    CameraParams ClampParams(CameraParams params) const;
    CameraParams ApplyAction(const CameraParams &current,
                             TuneAction action,
                             const Observation &observation) const;
    bool BrightnessReady(const ImageQuality &quality) const;
    bool ShouldRequestDecode(const Observation &observation) const;
    void AdvancePhaseAfterStep(StepDecision &decision, const Observation &observation);
    void HandleNoOp(StepDecision &decision, const Observation &observation);

    SessionConfig config_;
    SessionState state_;
    std::vector<CandidateRecord> candidates_;
    std::optional<CandidateRecord> best_candidate_;
    cv::Rect previous_roi_{};
};

} // namespace at

#endif // AT_AT_SESSION_H
