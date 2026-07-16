#include "at_session.h"

#include "core/candidate_store.h"
#include "core/vision/image_quality.h"
#include "core/vision/roi_tracker.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace at {
namespace {

// ---- 检测前置（detect-first）对焦策略的内部参数 ----
// 码区锁定判据：连续 kRoiLockHits 帧检测可用且与当前框 IoU >= kRoiLockIoU。
constexpr double kRoiLockIoU = 0.5;
constexpr int kRoiLockHits = 3;
// 锁定 ROI 与新检测框的平滑权重（新框占比）。
constexpr double kRoiBlend = 0.5;
// 粗对焦亮度探测档位：按就绪亮度窗口 [min_ready, max_ready] 的比例取目标亮度。
// 第 0 遍为窗口中值（与旧行为一致），未发现码区时再以偏暗/偏亮各扫一遍。
constexpr double kBrightnessProbeLevels[] = {0.5, 0.25, 0.8};
constexpr int kBrightnessProbePasses =
    static_cast<int>(sizeof(kBrightnessProbeLevels) / sizeof(kBrightnessProbeLevels[0]));
// 亮度收敛只发生在每遍对焦扫描开始之前（先粗调亮度，后整遍扫焦），
// 收敛步数上限取 flow.exposure_steps_per_profile，防止亮度无法达标时卡死。

bool IsExposureAction(TuneAction action)
{
    return action == TuneAction::IncreaseExposure || action == TuneAction::DecreaseExposure;
}

// 从候选记录尾部统计对焦阶段连续曝光调整次数（跨帧派生状态，避免新增成员破坏 ABI）。
int TrailingExposureTunes(const std::vector<CandidateRecord> &candidates)
{
    int count = 0;
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        if (it->phase == StepPhase::FocusTuneWithCoarseExposure && IsExposureAction(it->action)) {
            ++count;
        } else {
            break;
        }
    }
    return count;
}

// 当前检测框与最近历史检测框的连续命中次数（含当前帧）。
int ConsecutiveRoiHits(const std::vector<CandidateRecord> &candidates, const cv::Rect &current)
{
    int hits = 1;
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        if (it->heatmap.available && RectIoU(it->heatmap.roi, current) >= kRoiLockIoU) {
            ++hits;
        } else {
            break;
        }
    }
    return hits;
}

cv::Rect BlendRect(const cv::Rect &previous, const cv::Rect &current)
{
    const auto mix = [](int a, int b) {
        return static_cast<int>(std::lround((1.0 - kRoiBlend) * a + kRoiBlend * b));
    };
    return cv::Rect(mix(previous.x, current.x),
                    mix(previous.y, current.y),
                    mix(previous.width, current.width),
                    mix(previous.height, current.height));
}

// 对焦阶段清晰度最优的候选帧；锁定后优先只看有检测的帧（ROI 清晰度）。
const CandidateRecord *BestFocusCandidate(const std::vector<CandidateRecord> &candidates,
                                          bool prefer_detected)
{
    const CandidateRecord *best = nullptr;
    const CandidateRecord *best_detected = nullptr;
    for (const CandidateRecord &candidate : candidates) {
        if (candidate.phase != StepPhase::FocusTuneWithCoarseExposure) {
            continue;
        }
        if (!best || candidate.quality.sharpness > best->quality.sharpness) {
            best = &candidate;
        }
        if (candidate.heatmap.available &&
            (!best_detected || candidate.quality.sharpness > best_detected->quality.sharpness)) {
            best_detected = &candidate;
        }
    }
    if (prefer_detected && best_detected) {
        return best_detected;
    }
    return best;
}

// 每帧维护码区锁定框（locked_roi 非空 = 已锁定）：
// 已锁定时按 IoU 判据平滑更新；未锁定时按连续命中判据尝试锁定。
// 返回 true 表示本帧完成锁定。
bool UpdateRoiLock(const Observation &observation,
                   const std::vector<CandidateRecord> &candidates,
                   cv::Rect &locked_roi)
{
    if (!observation.heatmap.available || observation.heatmap.roi.empty()) {
        return false;
    }
    if (!locked_roi.empty()) {
        if (RectIoU(locked_roi, observation.heatmap.roi) >= kRoiLockIoU) {
            locked_roi = BlendRect(locked_roi, observation.heatmap.roi);
        }
        return false;
    }
    if (ConsecutiveRoiHits(candidates, observation.heatmap.roi) >= kRoiLockHits) {
        locked_roi = observation.heatmap.roi;
        return true;
    }
    return false;
}

// 对焦阶段单步计划：settle_action 为曝光动作时表示先收敛亮度，
// 否则本步把焦点移动到 focus_pos。note 追加到 decision.reason。
struct FocusPlan {
    TuneAction settle_action = TuneAction::Hold;
    int focus_pos = 0;
    std::string note;
};

int SettleBudget(const FlowConfig &flow)
{
    return std::max(1, flow.exposure_steps_per_profile);
}

// 已锁定码区：精扫开始前（精扫计数为 0 时）先基于 ROI 测光收敛曝光，
// 之后围绕历史最佳清晰度焦点做 [-half, +half] 窗口精扫，整遍不再调曝光。
FocusPlan PlanFineFocusOnLockedRoi(const SessionConfig &config,
                                   const SessionState &state,
                                   const std::vector<CandidateRecord> &candidates,
                                   const Observation &observation,
                                   bool brightness_ready)
{
    FocusPlan plan;
    if (state.exposure_tune_index == 0 && !brightness_ready &&
        TrailingExposureTunes(candidates) < SettleBudget(config.flow)) {
        plan.settle_action = observation.quality.brightness < config.flow.min_ready_brightness
                                 ? TuneAction::IncreaseExposure
                                 : TuneAction::DecreaseExposure;
        plan.note = "; settle roi brightness before fine focus";
        return plan;
    }

    const int coarse_steps = std::max(1, config.flow.focus_tune_steps);
    const int focus_range = config.camera.max_focus_pos - config.camera.min_focus_pos;
    const CandidateRecord *best = BestFocusCandidate(candidates, true);
    const int center = best ? best->params.focus_pos : observation.current_params.focus_pos;
    const int half = std::max(config.budget.focus_step, focus_range / (2 * coarse_steps));
    const int fine_steps = std::max(2, coarse_steps);
    const int fine_index = std::min(state.exposure_tune_index, fine_steps - 1);
    const double offset =
        -half + fine_index * (2.0 * half) / static_cast<double>(fine_steps - 1);
    plan.focus_pos = center + static_cast<int>(std::lround(offset));
    plan.note = "; fine focus on locked roi step " + std::to_string(fine_index) + "/" +
                std::to_string(fine_steps);
    return plan;
}

// 搜索中（未锁定）：每遍先把亮度收敛到该遍目标档，再整遍等距粗扫焦点，
// 全程由检测监测码区是否出现。关闭检测时退化为单遍（仅中值亮度档）。
FocusPlan PlanCoarseFocusSearch(const SessionConfig &config,
                                const SessionState &state,
                                const std::vector<CandidateRecord> &candidates,
                                const Observation &observation,
                                bool brightness_ready)
{
    FocusPlan plan;
    const int coarse_steps = std::max(1, config.flow.focus_tune_steps);
    const int max_passes = config.flow.enable_heatmap ? kBrightnessProbePasses : 1;
    const int pass = std::min(state.focus_tune_index / coarse_steps, max_passes - 1);
    const int slot = state.focus_tune_index % coarse_steps;

    const double window = config.flow.max_ready_brightness - config.flow.min_ready_brightness;
    const double target = config.flow.min_ready_brightness + kBrightnessProbeLevels[pass] * window;
    const double tolerance = std::max(12.0, window / 4.0);
    // 第 0 遍目标是窗口中值，直接用就绪判据；偏暗/偏亮遍改用目标档位容差判据。
    const bool brightness_ok =
        pass == 0 ? brightness_ready
                  : std::abs(observation.quality.brightness - target) <= tolerance &&
                        observation.quality.saturation_ratio <= config.flow.max_ready_saturation;

    if (slot == 0 && !brightness_ok && TrailingExposureTunes(candidates) < SettleBudget(config.flow)) {
        plan.settle_action = observation.quality.brightness < target
                                 ? TuneAction::IncreaseExposure
                                 : TuneAction::DecreaseExposure;
        plan.note = "; settle brightness for pass " + std::to_string(pass) + " target " +
                    std::to_string(static_cast<int>(target));
        return plan;
    }

    const int focus_range = config.camera.max_focus_pos - config.camera.min_focus_pos;
    plan.focus_pos = config.camera.min_focus_pos +
                     static_cast<int>(std::lround((slot + 0.5) * focus_range /
                                                  static_cast<double>(coarse_steps)));
    plan.note = "; coarse focus pass " + std::to_string(pass) + " slot " + std::to_string(slot);
    return plan;
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

    // Observe 仅是初始占位阶段：首帧直接进入对焦阶段。
    if (state_.phase == StepPhase::Observe || state_.step_index == 0) {
        state_.phase = StepPhase::FocusTuneWithCoarseExposure;
    }

    const Observation observation = BuildObservation(context);
    StepDecision decision = Decide(observation);

    // 调参动作却未改变任何参数（如已到边界钳位）视为 no-op，升级处理防止死循环。
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

    if (decision.finished) {
        state_.finish_reason = decision.finish_reason;
    }
    AdvancePhaseAfterStep(decision, observation);

    result.finished = decision.finished || state_.phase == StepPhase::Done;
    result.need_decode = decision.need_decode;
    result.next_params = ClampParams(decision.next_params);
    result.best_candidate = best_candidate_;
    result.trace.step_index = state_.step_index;
    result.trace.stage_step_count = state_.stage_step_count;
    result.trace.phase = state_.phase;
    result.trace.action = decision.action;
    result.trace.finish_reason = decision.finish_reason;
    result.trace.quality = observation.quality;
    result.trace.heatmap = observation.heatmap;
    result.trace.decode_used = state_.decode_used;
    result.trace.reason = decision.reason;

    // 全局步数保险丝（stage_step_count 跨阶段累计，防状态机循环不收敛）。
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
    // 码区已锁定而当前帧检测偶发丢失时，沿用锁定 ROI 测光，避免质量指标在
    // 全图/码区之间来回跳变干扰对焦与曝光判断。
    if (!context.heatmap.available && context.decode.regions.empty() && !previous_roi_.empty() &&
        !context.image.empty()) {
        const cv::Rect locked = ClipRect(previous_roi_, context.image.size());
        if (!locked.empty()) {
            observation.roi = locked;
        }
    }
    observation.quality = AnalyzeImageQuality(context.image, observation.roi);
    observation.heatmap = context.heatmap;
    observation.decode = context.decode;
    observation.preprocess = context.preprocess;
    return observation;
}

// Decide 只产出当前阶段内的动作与理由（decision.phase 恒等于进入本步时的
// state_.phase）；阶段转移统一由 AdvancePhaseAfterStep 负责。
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
            if (config_.flow.enable_heatmap &&
                UpdateRoiLock(observation, candidates_, previous_roi_)) {
                decision.reason += "; roi locked";
            }

            const FocusPlan plan =
                previous_roi_.empty()
                    ? PlanCoarseFocusSearch(config_, state_, candidates_, observation,
                                            BrightnessReady(observation.quality))
                    : PlanFineFocusOnLockedRoi(config_, state_, candidates_, observation,
                                               BrightnessReady(observation.quality));
            if (IsExposureAction(plan.settle_action)) {
                decision.action = plan.settle_action;
                decision.next_params =
                    ApplyAction(observation.current_params, decision.action, observation);
            } else {
                decision.action = TuneAction::AdjustFocus;
                decision.next_params = observation.current_params;
                decision.next_params.focus_pos = plan.focus_pos;
            }
            decision.reason += plan.note;
            break;
        }
        case StepPhase::ExposurePerLightProfile: {
            const auto &profiles = config_.flow.light_profiles;
            if (state_.light_profile_index < static_cast<int>(profiles.size())) {
                decision.action = TuneAction::AdjustLight;
                decision.next_params = observation.current_params;
                decision.next_params.lights = profiles[state_.light_profile_index];
                // 曝光阶段焦点钉在对焦阶段的最佳清晰度位置（锁定时为 ROI 清晰度最优）
                if (const CandidateRecord *best =
                        BestFocusCandidate(candidates_, !previous_roi_.empty())) {
                    decision.next_params.focus_pos = best->params.focus_pos;
                }
                if (!BrightnessReady(observation.quality)) {
                    decision.action = observation.quality.brightness < config_.flow.min_ready_brightness
                                          ? TuneAction::IncreaseExposure
                                          : TuneAction::DecreaseExposure;
                    decision.next_params = ApplyAction(decision.next_params, decision.action, observation);
                }
                decision.reason = "light profile " + std::to_string(state_.light_profile_index);
            } else {
                decision.action = TuneAction::Finish;
                decision.reason = "all light profiles done";
            }
            break;
        }
        case StepPhase::CandidateDecodeRanking: {
            if (!config_.flow.enable_decode_ranking) {
                decision.action = TuneAction::Hold;
                decision.reason = "decode ranking disabled";
                break;
            }
            if (state_.ranking_complete) {
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
            // 防御分支：正常流程不会以 Observe 进入 Decide（ProcessStep 已转换）。
            state_.phase = StepPhase::FocusTuneWithCoarseExposure;
            decision.phase = StepPhase::FocusTuneWithCoarseExposure;
            decision.action = TuneAction::AdjustFocus;
            decision.next_params = ApplyAction(observation.current_params, decision.action, observation);
            break;
    }

    return decision;
}

bool AtSession::SameParams(const CameraParams &lhs, const CameraParams &rhs) const
{
    return lhs.exp_time == rhs.exp_time && lhs.exp_gain == rhs.exp_gain &&
           lhs.focus_pos == rhs.focus_pos && lhs.lights == rhs.lights;
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
            // 优先加曝光时间；出现饱和（高光）时改为加增益。
            if (observation.quality.saturation_ratio < config_.flow.max_ready_saturation) {
                next.exp_time = std::min(config_.camera.max_exp_time, next.exp_time + budget.exposure_step);
            } else {
                next.exp_gain = std::min(config_.camera.max_exp_gain, next.exp_gain + budget.gain_step);
            }
            break;
        case TuneAction::DecreaseExposure:
            // 优先降增益（降噪），增益接近下限后再降曝光时间。
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

    switch (state_.phase) {
        case StepPhase::FocusTuneWithCoarseExposure: {
            const bool locked = !previous_roi_.empty();
            const int coarse_steps = std::max(1, config_.flow.focus_tune_steps);
            const int fine_steps = std::max(2, coarse_steps);
            const int max_passes = config_.flow.enable_heatmap ? kBrightnessProbePasses : 1;
            if (decision.action == TuneAction::AdjustFocus) {
                if (locked) {
                    // 锁定后 exposure_tune_index 复用为精细对焦步计数器，
                    // 进入曝光阶段时统一清零。
                    state_.exposure_tune_index += 1;
                } else {
                    state_.focus_tune_index += 1;
                }
            }
            const bool coarse_done = !locked && state_.focus_tune_index >= max_passes * coarse_steps;
            const bool fine_done = locked && state_.exposure_tune_index >= fine_steps;
            if (coarse_done || fine_done) {
                state_.phase = StepPhase::ExposurePerLightProfile;
                state_.light_profile_index = 0;
                state_.exposure_tune_index = 0;
            }
            break;
        }
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
        default:
            break;
    }
    decision.phase = state_.phase;
}

void AtSession::HandleNoOp(StepDecision &decision, const Observation &observation)
{
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
