#include "at_session.h"

#include "core/candidate_store.h"
#include "core/vision/image_quality.h"
#include "core/vision/roi_tracker.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace at {
namespace {

bool LightsEqual(const std::vector<int> &lhs, const std::vector<int> &rhs)
{
    return lhs == rhs;
}

} // namespace

std::string ToString(StepPhase phase)
{
    switch (phase) {
        case StepPhase::Observe:
            return "Observe";
        case StepPhase::FocusTuneWithCoarseExposure:
            return "FocusTuneWithCoarseExposure";
        case StepPhase::ExposurePerLightProfile:
            return "ExposurePerLightProfile";
        case StepPhase::CandidateDecodeRanking:
            return "CandidateDecodeRanking";
        case StepPhase::SelectBest:
            return "SelectBest";
        case StepPhase::Done:
            return "Done";
    }
    return "Unknown";
}

std::string ToString(TuneAction action)
{
    switch (action) {
        case TuneAction::Hold:
            return "Hold";
        case TuneAction::IncreaseExposure:
            return "IncreaseExposure";
        case TuneAction::DecreaseExposure:
            return "DecreaseExposure";
        case TuneAction::AdjustLight:
            return "AdjustLight";
        case TuneAction::AdjustFocus:
            return "AdjustFocus";
        case TuneAction::RequestDecode:
            return "RequestDecode";
        case TuneAction::Finish:
            return "Finish";
    }
    return "Unknown";
}

std::string ToString(FinishReason reason)
{
    switch (reason) {
        case FinishReason::None:
            return "None";
        case FinishReason::DecodeSuccess:
            return "DecodeSuccess";
        case FinishReason::QualityStable:
            return "QualityStable";
        case FinishReason::BestCandidateFound:
            return "BestCandidateFound";
        case FinishReason::BudgetExhausted:
            return "BudgetExhausted";
        case FinishReason::SearchFailed:
            return "SearchFailed";
        case FinishReason::ExternalStepLimit:
            return "ExternalStepLimit";
        case FinishReason::NoOpEscalation:
            return "NoOpEscalation";
        case FinishReason::Error:
            return "Error";
    }
    return "Unknown";
}

AtSession::AtSession(SessionConfig config)
    : config_(std::move(config))
{
}

void AtSession::Reset()
{
    state_ = {};
    candidates_.clear();
    best_candidate_.reset();
    previous_roi_ = {};
}

StepResult AtSession::ProcessStep(const FrameContext &context)
{
    StepResult result;
    result.next_params = context.current_params;

    if (context.decode.attempted) {
        state_.decode_used += 1;
    }

    if (state_.phase == StepPhase::Observe || state_.step_index == 0) {
        state_.phase = StepPhase::FocusTuneWithCoarseExposure;
    }

    const Observation observation = BuildObservation(context);
    StepDecision decision = Decide(observation);

    if (decision.action != TuneAction::Hold && decision.action != TuneAction::RequestDecode &&
        decision.action != TuneAction::Finish &&
        SameParams(decision.next_params, context.current_params)) {
        HandleNoOp(decision, observation);
    }

    const CandidateRecord candidate = MakeCandidate(context,
                                                    observation.roi,
                                                    observation.quality,
                                                    decision.phase,
                                                    decision.action);
    candidates_.push_back(candidate);
    UpdateBestCandidate(candidate, best_candidate_);

    state_.step_index += 1;
    state_.stage_step_count += 1;
    state_.phase = decision.phase;

    if (decision.finished) {
        state_.phase = StepPhase::Done;
        state_.finish_reason = decision.finish_reason;
    }

    AdvancePhaseAfterStep(decision, observation);

    result.finished = decision.finished || state_.phase == StepPhase::Done;
    result.need_decode = decision.need_decode;
    result.next_params = ClampParams(decision.next_params);
    result.best_candidate = best_candidate_;
    result.trace.trace_version = kTraceVersion;
    result.trace.step_index = state_.step_index;
    result.trace.stage_step_count = state_.stage_step_count;
    result.trace.phase = state_.phase;
    result.trace.action = decision.action;
    result.trace.finish_reason = decision.finish_reason;
    result.trace.quality = observation.quality;
    result.trace.heatmap = observation.heatmap;
    result.trace.decode = observation.decode;
    result.trace.preprocess = observation.preprocess;
    result.trace.decode_used = state_.decode_used;
    result.trace.decode_budget = config_.budget.decode_budget;
    result.trace.active_max_steps = config_.budget.max_steps;
    result.trace.reason = decision.reason;

    if (state_.stage_step_count >= config_.budget.max_steps && !result.finished) {
        result.finished = true;
        result.trace.finish_reason = FinishReason::BudgetExhausted;
        state_.finish_reason = FinishReason::BudgetExhausted;
        state_.phase = StepPhase::Done;
        result.trace.phase = StepPhase::Done;
        result.trace.reason += "; budget exhausted";
    }

    return result;
}

const std::vector<CandidateRecord> &AtSession::Candidates() const
{
    return candidates_;
}

Observation AtSession::BuildObservation(const FrameContext &context) const
{
    Observation observation;
    observation.current_params = context.current_params;
    observation.roi = SelectAnalysisRoi(context);
    observation.quality = AnalyzeImageQuality(context.image, observation.roi);
    observation.heatmap = context.heatmap;
    observation.decode = context.decode;
    observation.preprocess = context.preprocess;
    return observation;
}

StepDecision AtSession::Decide(const Observation &observation)
{
    StepDecision decision;
    decision.phase = state_.phase;
    decision.next_params = observation.current_params;
    decision.reason = "phase=" + ToString(state_.phase);

    if (state_.phase == StepPhase::Done) {
        decision.finished = true;
        decision.action = TuneAction::Finish;
        decision.finish_reason = state_.finish_reason;
        return decision;
    }

    switch (state_.phase) {
        case StepPhase::FocusTuneWithCoarseExposure: {
            if (!BrightnessReady(observation.quality)) {
                decision.action = observation.quality.brightness < config_.flow.min_ready_brightness
                                      ? TuneAction::IncreaseExposure
                                      : TuneAction::DecreaseExposure;
                decision.reason = "coarse exposure before focus sweep";
            } else {
                decision.action = TuneAction::AdjustFocus;
                decision.reason = "focus sweep";
            }
            decision.next_params = ApplyAction(observation.current_params, decision.action, observation);
            if (state_.focus_tune_index + 1 >= config_.flow.focus_tune_steps) {
                decision.reason += "; focus sweep complete";
            }
            break;
        }
        case StepPhase::ExposurePerLightProfile: {
            const auto &profiles = config_.flow.light_profiles;
            if (state_.light_profile_index < static_cast<int>(profiles.size())) {
                decision.action = TuneAction::AdjustLight;
                decision.next_params = observation.current_params;
                decision.next_params.lights = profiles[state_.light_profile_index];
                if (!BrightnessReady(observation.quality)) {
                    decision.action = observation.quality.brightness < config_.flow.min_ready_brightness
                                          ? TuneAction::IncreaseExposure
                                          : TuneAction::DecreaseExposure;
                    decision.next_params = ApplyAction(decision.next_params, decision.action, observation);
                }
                decision.reason = "light profile " + std::to_string(state_.light_profile_index);
            } else {
                decision.phase = StepPhase::SelectBest;
                decision.action = TuneAction::Finish;
                decision.reason = "all light profiles done";
            }
            break;
        }
        case StepPhase::CandidateDecodeRanking: {
            if (!config_.flow.enable_decode_ranking) {
                decision.phase = StepPhase::SelectBest;
                decision.action = TuneAction::Hold;
                decision.reason = "decode ranking disabled";
                break;
            }
            if (state_.ranking_complete) {
                decision.phase = StepPhase::SelectBest;
                decision.action = TuneAction::Hold;
                decision.reason = "ranking complete";
                break;
            }
            if (ShouldRequestDecode(observation)) {
                decision.action = TuneAction::RequestDecode;
                decision.need_decode = true;
                decision.reason = "request decode for candidate ranking";
            } else if (observation.decode.attempted) {
                state_.ranking_complete = true;
                decision.phase = StepPhase::SelectBest;
                decision.reason = "decode feedback received";
            } else {
                decision.action = TuneAction::Hold;
                decision.reason = "waiting decode gate";
            }
            break;
        }
        case StepPhase::SelectBest: {
            decision.action = TuneAction::Finish;
            decision.finished = true;
            decision.finish_reason = best_candidate_ ? FinishReason::BestCandidateFound
                                                     : FinishReason::SearchFailed;
            if (best_candidate_) {
                decision.next_params = best_candidate_->params;
            }
            decision.reason = "select best candidate";
            break;
        }
        case StepPhase::Observe:
        default:
            state_.phase = StepPhase::FocusTuneWithCoarseExposure;
            decision.phase = StepPhase::FocusTuneWithCoarseExposure;
            decision.action = TuneAction::AdjustFocus;
            decision.next_params = ApplyAction(observation.current_params, decision.action, observation);
            break;
    }

    decision.phase = state_.phase;
    return decision;
}

bool AtSession::SameParams(const CameraParams &lhs, const CameraParams &rhs) const
{
    return lhs.exp_time == rhs.exp_time && lhs.exp_gain == rhs.exp_gain &&
           lhs.focus_pos == rhs.focus_pos && LightsEqual(lhs.lights, rhs.lights);
}

CameraParams AtSession::ClampParams(CameraParams params) const
{
    params.exp_time = std::clamp(params.exp_time,
                                 config_.camera.min_exp_time,
                                 config_.camera.max_exp_time);
    params.exp_gain = std::clamp(params.exp_gain,
                                 config_.camera.min_exp_gain,
                                 config_.camera.max_exp_gain);
    params.focus_pos = std::clamp(params.focus_pos,
                                  config_.camera.min_focus_pos,
                                  config_.camera.max_focus_pos);
    return params;
}

CameraParams AtSession::ApplyAction(const CameraParams &current,
                                         TuneAction action,
                                         const Observation &observation) const
{
    CameraParams next = current;
    const StrategyBudget &budget = config_.budget;

    switch (action) {
        case TuneAction::IncreaseExposure:
            if (observation.quality.saturation_ratio < config_.flow.max_ready_saturation) {
                next.exp_time = std::min(config_.camera.max_exp_time, next.exp_time + budget.exposure_step);
            } else {
                next.exp_gain = std::min(config_.camera.max_exp_gain, next.exp_gain + budget.gain_step);
            }
            break;
        case TuneAction::DecreaseExposure:
            if (next.exp_gain > config_.camera.min_exp_gain + budget.gain_step) {
                next.exp_gain = std::max(config_.camera.min_exp_gain, next.exp_gain - budget.gain_step);
            } else {
                next.exp_time = std::max(config_.camera.min_exp_time, next.exp_time - budget.exposure_step);
            }
            break;
        case TuneAction::AdjustFocus:
            next.focus_pos = std::min(config_.camera.max_focus_pos,
                                      next.focus_pos + budget.focus_step);
            break;
        case TuneAction::AdjustLight:
            break;
        default:
            break;
    }
    return ClampParams(next);
}

bool AtSession::BrightnessReady(const ImageQuality &quality) const
{
    return quality.brightness >= config_.flow.min_ready_brightness &&
           quality.brightness <= config_.flow.max_ready_brightness &&
           quality.saturation_ratio <= config_.flow.max_ready_saturation;
}

bool AtSession::ShouldRequestDecode(const Observation &observation) const
{
    if (state_.decode_used >= config_.budget.decode_budget) {
        return false;
    }
    if (!BrightnessReady(observation.quality)) {
        return false;
    }
    if (config_.flow.enable_heatmap && observation.heatmap.available &&
        observation.heatmap.confidence < 0.5) {
        return false;
    }
    return true;
}

void AtSession::AdvancePhaseAfterStep(StepDecision &decision, const Observation &observation)
{
    if (decision.finished) {
        state_.phase = StepPhase::Done;
        decision.phase = state_.phase;
        return;
    }

    const StepPhase previous_phase = state_.phase;

    switch (previous_phase) {
        case StepPhase::FocusTuneWithCoarseExposure:
            if (decision.action == TuneAction::AdjustFocus ||
                decision.action == TuneAction::IncreaseExposure ||
                decision.action == TuneAction::DecreaseExposure) {
                state_.focus_tune_index += 1;
            }
            if (state_.focus_tune_index >= config_.flow.focus_tune_steps) {
                state_.phase = StepPhase::ExposurePerLightProfile;
                state_.light_profile_index = 0;
                state_.exposure_tune_index = 0;
            }
            break;
        case StepPhase::ExposurePerLightProfile:
            state_.exposure_tune_index += 1;
            if (BrightnessReady(observation.quality) ||
                state_.exposure_tune_index >= config_.flow.exposure_steps_per_profile) {
                state_.exposure_tune_index = 0;
                state_.light_profile_index += 1;
            }
            if (state_.light_profile_index >= static_cast<int>(config_.flow.light_profiles.size())) {
                state_.phase = config_.flow.enable_decode_ranking
                                   ? StepPhase::CandidateDecodeRanking
                                   : StepPhase::SelectBest;
            }
            break;
        case StepPhase::CandidateDecodeRanking:
            if (state_.ranking_complete || !config_.flow.enable_decode_ranking) {
                state_.phase = StepPhase::SelectBest;
            }
            break;
        case StepPhase::SelectBest:
            if (decision.finished) {
                state_.phase = StepPhase::Done;
            }
            break;
        default:
            break;
    }
    decision.phase = state_.phase;
}

void AtSession::HandleNoOp(StepDecision &decision, const Observation &observation)
{
    (void)observation;
    decision.reason += "; no-op escalation";
    decision.action = TuneAction::AdjustFocus;
    decision.next_params = ApplyAction(decision.next_params, TuneAction::AdjustFocus, observation);
    if (state_.phase == StepPhase::FocusTuneWithCoarseExposure) {
        state_.focus_tune_index += 1;
    }
}

#ifndef AT_VERSION_STR
#define AT_VERSION_STR "0.0.0-dev"
#endif

const char *GetLibVersion()
{
    return AT_VERSION_STR;
}

} // namespace at
