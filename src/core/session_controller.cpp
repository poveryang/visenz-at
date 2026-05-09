#include "at/at_session.h"

#include "core/candidate_pool.h"
#include "vision/image_quality.h"
#include "vision/roi_tracker.h"

#include <algorithm>
#include <utility>

namespace at {

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
        case StepPhase::Tune:
            return "Tune";
        case StepPhase::DecodeVerify:
            return "DecodeVerify";
        case StepPhase::Refine:
            return "Refine";
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

ATSession::ATSession(SessionConfig config)
    : config_(std::move(config))
{
}

void ATSession::Reset()
{
    candidates_.clear();
    best_candidate_.reset();
    previous_roi_ = {};
    step_index_ = 0;
    decode_used_ = 0;
    phase_ = StepPhase::Observe;
}

StepResult ATSession::Step(const FrameInput &input)
{
    StepResult result;
    result.next_params = input.current_params;

    const cv::Rect roi = SelectAnalysisRoi(input);
    HeatmapObservation heatmap = input.heatmap;
    if (!roi.empty() && !previous_roi_.empty()) {
        heatmap.stability = RectIoU(previous_roi_, roi);
    }

    const ImageQuality quality = AnalyzeImageQuality(input.image, roi);
    const SceneDifficulty difficulty = ClassifyScene(quality, heatmap, input.decode);
    const StrategyBudget &budget = BudgetFor(difficulty);
    const bool need_decode = ShouldDecode(quality, heatmap, input.decode, budget);
    const TuneAction selected_action = need_decode
                                           ? TuneAction::RequestDecode
                                           : SelectAction(quality, heatmap, input.decode, budget);

    if (input.decode.attempted) {
        decode_used_ += 1;
    }

    if (input.decode.success) {
        phase_ = budget.allow_post_decode_refine ? StepPhase::Refine : StepPhase::Done;
    } else if (need_decode) {
        phase_ = StepPhase::DecodeVerify;
    } else if (step_index_ == 0) {
        phase_ = StepPhase::Observe;
    } else {
        phase_ = StepPhase::Tune;
    }

    CandidateRecord candidate = MakeCandidate(input, roi, quality, difficulty, phase_, selected_action);
    UpdateBestCandidate(candidate, best_candidate_);
    candidates_.push_back(candidate);

    result.need_decode = need_decode;
    result.next_params = BuildNextParams(input.current_params, selected_action, budget);
    result.best_candidate = best_candidate_;
    result.finished = phase_ == StepPhase::Done || step_index_ + 1 >= budget.max_steps;
    if (result.finished) {
        phase_ = StepPhase::Done;
        result.next_params = best_candidate_ ? best_candidate_->params : input.current_params;
    }

    result.trace.step_index = step_index_;
    result.trace.phase = phase_;
    result.trace.difficulty = difficulty;
    result.trace.action = result.finished ? TuneAction::Finish : selected_action;
    result.trace.quality = quality;
    result.trace.heatmap = heatmap;
    result.trace.decode_used = decode_used_;
    result.trace.decode_budget = budget.decode_budget;
    result.trace.reason = candidate.reason;

    previous_roi_ = roi;
    step_index_ += 1;
    return result;
}

const std::vector<CandidateRecord> &ATSession::Candidates() const
{
    return candidates_;
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

TuneAction ATSession::SelectAction(const ImageQuality &quality,
                                   const HeatmapObservation &heatmap,
                                   const DecodeFeedback &decode,
                                   const StrategyBudget &budget) const
{
    if (decode.success) {
        return budget.allow_post_decode_refine ? TuneAction::AdjustFocus : TuneAction::Finish;
    }
    if (quality.saturation_ratio > 0.12 || quality.brightness > 190.0) {
        return TuneAction::DecreaseExposure;
    }
    if (quality.brightness < 55.0) {
        return TuneAction::IncreaseExposure;
    }
    if (!heatmap.available && budget.allow_search_expansion) {
        return TuneAction::AdjustLight;
    }
    return TuneAction::AdjustFocus;
}

CameraParams ATSession::BuildNextParams(const CameraParams &current,
                                        TuneAction action,
                                        const StrategyBudget &budget) const
{
    CameraParams next = current;
    const CameraRange &camera = config_.camera;

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
        case TuneAction::AdjustLight:
            for (auto &light : next.lights) {
                light = camera.max_light;
            }
            break;
        case TuneAction::AdjustFocus: {
            const int direction = (step_index_ % 2 == 0) ? 1 : -1;
            next.focus_pos = std::clamp(current.focus_pos + direction * budget.focus_step,
                                        camera.min_focus_pos,
                                        camera.max_focus_pos);
            break;
        }
        case TuneAction::RequestDecode:
        case TuneAction::Finish:
        case TuneAction::Hold:
            break;
    }
    return next;
}

bool ATSession::ShouldDecode(const ImageQuality &quality,
                             const HeatmapObservation &heatmap,
                             const DecodeFeedback &decode,
                             const StrategyBudget &budget) const
{
    if (decode.success || decode_used_ >= budget.decode_budget) {
        return false;
    }
    if (heatmap.available && heatmap.confidence > 0.60 && quality.saturation_ratio < 0.10) {
        return true;
    }
    return step_index_ > 0 &&
           step_index_ % 8 == 0 &&
           quality.brightness >= 55.0 &&
           quality.brightness <= 190.0;
}

} // namespace at
