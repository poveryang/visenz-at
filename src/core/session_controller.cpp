#include "at/at_session.h"

#include "core/candidate_pool.h"
#include "vision/image_quality.h"
#include "vision/roi_tracker.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace at {
namespace {

int DifficultyRank(SceneDifficulty difficulty)
{
    switch (difficulty) {
        case SceneDifficulty::Simple:
            return 0;
        case SceneDifficulty::Normal:
            return 1;
        case SceneDifficulty::Hard:
            return 2;
    }
    return 1;
}

SceneDifficulty MaxDifficulty(SceneDifficulty lhs, SceneDifficulty rhs)
{
    return DifficultyRank(lhs) >= DifficultyRank(rhs) ? lhs : rhs;
}

bool SameExposureContext(const CameraParams &lhs, const CameraParams &rhs)
{
    return lhs.exp_gain == rhs.exp_gain && lhs.lights == rhs.lights;
}

} // namespace

std::string ToString(SceneDifficulty difficulty)
{
    switch (difficulty) {
        case SceneDifficulty::Simple:
            return "Simple";
        case SceneDifficulty::Normal:
            return "Normal";
        case SceneDifficulty::Hard:
            return "Hard";
    }
    return "Unknown";
}

std::string ToString(StepPhase phase)
{
    switch (phase) {
        case StepPhase::Observe:
            return "Observe";
        case StepPhase::BrightnessPrecondition:
            return "BrightnessPrecondition";
        case StepPhase::RoiObserve:
            return "RoiObserve";
        case StepPhase::FocusExplore:
            return "FocusExplore";
        case StepPhase::LocalRefine:
            return "LocalRefine";
        case StepPhase::DecodeVerify:
            return "DecodeVerify";
        case StepPhase::PostDecodeRefine:
            return "PostDecodeRefine";
        case StepPhase::FallbackSearch:
            return "FallbackSearch";
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
        case FinishReason::Error:
            return "Error";
    }
    return "Unknown";
}

ATSession::ATSession(SessionConfig config)
    : config_(std::move(config))
{
}

void ATSession::Reset()
{
    candidates_.clear();
    best_candidate_.reset();
    previous_roi_ = {};
    state_ = {};
    exposure_samples_.clear();
}

StepResult ATSession::Step(const FrameInput &input)
{
    StepResult result;
    result.next_params = input.current_params;

    if (input.decode.attempted) {
        state_.decode_used += 1;
    }

    const Observation observation = BuildObservation(input);
    UpdateDifficultyState(observation.difficulty);
    const StrategyBudget &budget = ActiveBudget();
    TuningDecision decision = Decide(observation, budget);

    CandidateRecord candidate = MakeCandidate(input,
                                              observation.roi,
                                              observation.quality,
                                              observation.difficulty,
                                              decision.phase,
                                              decision.action);
    candidate.heatmap = observation.heatmap;
    UpdateBestCandidate(candidate, best_candidate_);
    candidates_.push_back(candidate);

    if (!decision.finished && state_.step_index + 1 >= budget.max_steps) {
        decision.finished = true;
        decision.finish_reason = FinishReason::BudgetExhausted;
        decision.action = TuneAction::Finish;
        decision.phase = StepPhase::Done;
        decision.need_decode = false;
        decision.reason = candidate.reason + " finish=budget_exhausted";
        decision.next_params = best_candidate_ ? best_candidate_->params : input.current_params;
    }

    UpdateStageState(decision.phase);

    result.need_decode = decision.need_decode;
    result.next_params = decision.next_params;
    result.best_candidate = best_candidate_;
    result.finished = decision.finished;

    if (decision.finished) {
        state_.finish_reason = decision.finish_reason;
    }

    result.trace.step_index = state_.step_index;
    result.trace.stage_step_count = state_.stage_step_count;
    result.trace.phase = state_.phase;
    result.trace.difficulty = observation.difficulty;
    result.trace.historical_max_difficulty = state_.historical_max_difficulty;
    result.trace.action = decision.action;
    result.trace.finish_reason = decision.finish_reason;
    result.trace.quality = observation.quality;
    result.trace.heatmap = observation.heatmap;
    result.trace.decode_used = state_.decode_used;
    result.trace.decode_budget = budget.decode_budget;
    result.trace.active_max_steps = budget.max_steps;
    result.trace.reason = decision.reason.empty() ? candidate.reason : decision.reason;

    RecordExposureSample(observation);
    previous_roi_ = observation.roi;
    state_.step_index += 1;
    return result;
}

const std::vector<CandidateRecord> &ATSession::Candidates() const
{
    return candidates_;
}

Observation ATSession::BuildObservation(const FrameInput &input) const
{
    Observation observation;
    observation.current_params = input.current_params;
    observation.roi = SelectAnalysisRoi(input);
    observation.heatmap = input.heatmap;
    if (!observation.roi.empty() && !previous_roi_.empty()) {
        observation.heatmap.stability = RectIoU(previous_roi_, observation.roi);
    }
    observation.quality = AnalyzeImageQuality(input.image, observation.roi);
    observation.decode = input.decode;
    observation.difficulty = ClassifyScene(observation.quality,
                                           observation.heatmap,
                                           observation.decode);
    return observation;
}

void ATSession::UpdateDifficultyState(SceneDifficulty difficulty)
{
    state_.historical_max_difficulty = MaxDifficulty(state_.historical_max_difficulty, difficulty);
}

const StrategyBudget &ATSession::ActiveBudget() const
{
    return BudgetFor(state_.historical_max_difficulty);
}

TuningDecision ATSession::Decide(const Observation &observation,
                                 const StrategyBudget &budget) const
{
    TuningDecision decision;
    decision.next_params = observation.current_params;

    if (observation.decode.success) {
        decision.phase = StepPhase::Done;
        decision.action = TuneAction::Finish;
        decision.finished = true;
        decision.finish_reason = FinishReason::DecodeSuccess;
        decision.reason = "decode success";
        return decision;
    }

    decision.phase = SelectConfiguredPhase(observation, budget);
    if (decision.phase == StepPhase::Done) {
        decision.action = TuneAction::Finish;
        decision.finished = true;
        decision.finish_reason = FinishReason::BestCandidateFound;
        decision.reason = "configured flow completed";
        return decision;
    }

    decision.action = SelectActionForPhase(decision.phase, observation);
    decision.need_decode = decision.action == TuneAction::RequestDecode;
    decision.next_params = BuildNextParams(observation.current_params,
                                           decision.action,
                                           observation.quality,
                                           budget);

    if (!SameParams(decision.next_params, observation.current_params) ||
        decision.action == TuneAction::Hold ||
        decision.action == TuneAction::RequestDecode) {
        return decision;
    }

    if (decision.phase != StepPhase::FocusExplore &&
        IsStageEnabled(StepPhase::FocusExplore) &&
        StageHasRemainingSteps(StepPhase::FocusExplore, config_.flow.focus_explore_steps)) {
        decision.phase = StepPhase::FocusExplore;
        decision.action = TuneAction::AdjustFocus;
        decision.next_params = BuildNextParams(observation.current_params,
                                               decision.action,
                                               observation.quality,
                                               budget);
        decision.reason = "fallback to focus after no-op action";
        return decision;
    }

    decision.phase = StepPhase::LocalRefine;
    decision.action = TuneAction::Hold;
    decision.reason = "hold after no-op action";
    return decision;
}

StepPhase ATSession::SelectConfiguredPhase(const Observation &observation,
                                           const StrategyBudget &budget) const
{
    if (NeedsBrightnessPrecondition(observation.quality) &&
        IsStageEnabled(StepPhase::BrightnessPrecondition)) {
        return StepPhase::BrightnessPrecondition;
    }
    if (ShouldDecode(observation.quality,
                     observation.heatmap,
                     observation.decode,
                     budget) &&
        IsStageEnabled(StepPhase::DecodeVerify)) {
        return StepPhase::DecodeVerify;
    }
    if (observation.heatmap.available &&
        config_.flow.enable_roi_observe &&
        IsStageEnabled(StepPhase::RoiObserve)) {
        return StepPhase::RoiObserve;
    }
    if (IsStageEnabled(StepPhase::FocusExplore) &&
        StageHasRemainingSteps(StepPhase::FocusExplore, config_.flow.focus_explore_steps)) {
        return StepPhase::FocusExplore;
    }
    if (IsStageEnabled(StepPhase::LocalRefine) &&
        StageHasRemainingSteps(StepPhase::LocalRefine, config_.flow.local_refine_steps)) {
        return StepPhase::LocalRefine;
    }
    return StepPhase::Done;
}

TuneAction ATSession::SelectActionForPhase(StepPhase phase,
                                           const Observation &observation) const
{
    switch (phase) {
        case StepPhase::BrightnessPrecondition:
            if (observation.quality.saturation_ratio > config_.flow.max_ready_saturation ||
                observation.quality.brightness > config_.flow.max_ready_brightness) {
                return TuneAction::DecreaseExposure;
            }
            return TuneAction::IncreaseExposure;
        case StepPhase::RoiObserve:
            return TuneAction::AdjustLight;
        case StepPhase::FocusExplore:
            return TuneAction::AdjustFocus;
        case StepPhase::DecodeVerify:
            return TuneAction::RequestDecode;
        case StepPhase::LocalRefine:
            return TuneAction::Hold;
        case StepPhase::Observe:
        case StepPhase::PostDecodeRefine:
        case StepPhase::FallbackSearch:
        case StepPhase::Done:
            break;
    }
    return TuneAction::Hold;
}

bool ATSession::IsStageEnabled(StepPhase phase) const
{
    return std::find(config_.flow.stages.begin(), config_.flow.stages.end(), phase) !=
           config_.flow.stages.end();
}

bool ATSession::StageHasRemainingSteps(StepPhase phase, int max_steps) const
{
    if (max_steps <= 0) {
        return false;
    }
    if (state_.phase != phase) {
        return true;
    }
    return state_.stage_step_count + 1 < max_steps;
}

bool ATSession::NeedsBrightnessPrecondition(const ImageQuality &quality) const
{
    return quality.brightness < config_.flow.min_ready_brightness ||
           quality.brightness > config_.flow.max_ready_brightness ||
           quality.saturation_ratio > config_.flow.max_ready_saturation;
}

bool ATSession::SameParams(const CameraParams &lhs, const CameraParams &rhs) const
{
    return lhs.exp_time == rhs.exp_time &&
           lhs.exp_gain == rhs.exp_gain &&
           lhs.focus_pos == rhs.focus_pos &&
           lhs.lights == rhs.lights;
}

void ATSession::UpdateStageState(StepPhase phase)
{
    if (phase == state_.phase) {
        state_.stage_step_count += 1;
    } else {
        state_.phase = phase;
        state_.stage_step_count = 0;
    }
}

SceneDifficulty ATSession::ClassifyScene(const ImageQuality &quality,
                                         const HeatmapObservation &heatmap,
                                         const DecodeFeedback &decode) const
{
    if (decode.success) {
        return SceneDifficulty::Simple;
    }
    if (heatmap.available && heatmap.confidence >= 0.65 &&
        quality.brightness >= 60.0 && quality.brightness <= 180.0 &&
        quality.saturation_ratio < 0.08) {
        return SceneDifficulty::Simple;
    }
    if (quality.brightness < 35.0 ||
        quality.brightness > 220.0 ||
        quality.saturation_ratio > 0.20 ||
        quality.contrast < 4.0) {
        return SceneDifficulty::Hard;
    }
    return SceneDifficulty::Normal;
}

const StrategyBudget &ATSession::BudgetFor(SceneDifficulty difficulty) const
{
    switch (difficulty) {
        case SceneDifficulty::Simple:
            return config_.simple;
        case SceneDifficulty::Hard:
            return config_.hard;
        case SceneDifficulty::Normal:
            return config_.normal;
    }
    return config_.normal;
}

CameraParams ATSession::BuildNextParams(const CameraParams &current,
                                        TuneAction action,
                                        const ImageQuality &quality,
                                        const StrategyBudget &budget) const
{
    CameraParams next = current;
    const CameraRange &camera = config_.camera;

    switch (action) {
        case TuneAction::IncreaseExposure:
        case TuneAction::DecreaseExposure:
            return BuildBrightnessPreconditionParams(current, action, quality, budget);
        case TuneAction::AdjustLight:
            for (auto &light : next.lights) {
                light = camera.max_light;
            }
            break;
        case TuneAction::AdjustFocus: {
            if (current.focus_pos + budget.focus_step <= camera.max_focus_pos) {
                next.focus_pos = current.focus_pos + budget.focus_step;
            } else {
                next.focus_pos = std::max(camera.min_focus_pos, current.focus_pos - budget.focus_step);
            }
            break;
        }
        case TuneAction::RequestDecode:
        case TuneAction::Finish:
        case TuneAction::Hold:
            break;
    }
    return next;
}

CameraParams ATSession::BuildBrightnessPreconditionParams(const CameraParams &current,
                                                          TuneAction action,
                                                          const ImageQuality &quality,
                                                          const StrategyBudget &budget) const
{
    CameraParams next = current;
    const CameraRange &camera = config_.camera;
    constexpr double kTargetBrightness = 110.0;

    auto fallback_step = [&]() {
        switch (action) {
        case TuneAction::IncreaseExposure:
            if (current.exp_time < camera.max_exp_time) {
                next.exp_time = std::min(camera.max_exp_time, current.exp_time + budget.exposure_step);
            } else {
                next.exp_gain = std::min(camera.max_exp_gain, current.exp_gain + budget.gain_step);
            }
            break;
        case TuneAction::DecreaseExposure:
            if (current.exp_gain > camera.min_exp_gain) {
                next.exp_gain = std::max(camera.min_exp_gain, current.exp_gain - budget.gain_step);
            } else {
                next.exp_time = std::max(camera.min_exp_time, current.exp_time - budget.exposure_step);
            }
            break;
        case TuneAction::RequestDecode:
        case TuneAction::Finish:
        case TuneAction::AdjustLight:
        case TuneAction::AdjustFocus:
        case TuneAction::Hold:
            break;
        }
    };

    if (quality.brightness <= 1.0 || quality.saturation_ratio > 0.20) {
        fallback_step();
        return next;
    }

    std::optional<double> estimated_exp_time;
    for (auto it = exposure_samples_.rbegin(); it != exposure_samples_.rend(); ++it) {
        if (!SameExposureContext(it->params, current) ||
            it->params.exp_time == current.exp_time ||
            it->brightness <= 1.0 ||
            it->saturation_ratio > 0.20) {
            continue;
        }

        const double slope = (quality.brightness - it->brightness) /
                             static_cast<double>(current.exp_time - it->params.exp_time);
        if (std::abs(slope) > 1e-6) {
            estimated_exp_time = current.exp_time + (kTargetBrightness - quality.brightness) / slope;
            break;
        }
    }

    if (!estimated_exp_time.has_value()) {
        estimated_exp_time = static_cast<double>(current.exp_time) *
                             (kTargetBrightness / quality.brightness);
    }

    const int estimated = std::clamp(static_cast<int>(std::lround(*estimated_exp_time)),
                                     camera.min_exp_time,
                                     camera.max_exp_time);

    if (action == TuneAction::IncreaseExposure && estimated > current.exp_time) {
        next.exp_time = estimated;
        return next;
    }
    if (action == TuneAction::DecreaseExposure && estimated < current.exp_time) {
        next.exp_time = estimated;
        return next;
    }

    fallback_step();
    return next;
}

bool ATSession::ShouldDecode(const ImageQuality &quality,
                             const HeatmapObservation &heatmap,
                             const DecodeFeedback &decode,
                             const StrategyBudget &budget) const
{
    if (decode.success || state_.decode_used >= budget.decode_budget) {
        return false;
    }
    if (config_.flow.enable_heatmap_decode &&
        heatmap.available &&
        heatmap.confidence > 0.60 &&
        quality.saturation_ratio < 0.10) {
        return true;
    }
    if (!config_.flow.enable_periodic_decode ||
        config_.flow.decode_interval_steps <= 0) {
        return false;
    }
    return state_.step_index > 0 &&
           state_.step_index % config_.flow.decode_interval_steps == 0 &&
           quality.brightness >= config_.flow.min_ready_brightness &&
           quality.brightness <= config_.flow.max_ready_brightness;
}

void ATSession::RecordExposureSample(const Observation &observation)
{
    exposure_samples_.push_back({
        observation.current_params,
        observation.quality.brightness,
        observation.quality.saturation_ratio,
    });
    constexpr std::size_t kMaxExposureSamples = 8;
    if (exposure_samples_.size() > kMaxExposureSamples) {
        exposure_samples_.erase(exposure_samples_.begin(),
                                exposure_samples_.end() - kMaxExposureSamples);
    }
}

} // namespace at
