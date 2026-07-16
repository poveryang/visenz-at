#include "at_session.h"

#include "core/candidate_store.h"
#include "core/vision/image_quality.h"
#include "core/vision/roi_tracker.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace at {
namespace {

// ============================================================================
// 面向解码的自动图像调节流程（v5.4，阶段枚举与公共接口不变）
//
// FocusTuneWithCoarseExposure 内部细分为四个子阶段：
//   CoarseSweep  对焦前先把亮度收敛到就绪窗口中值（比例式 AE），
//                然后整遍等距粗对焦扫描（不依赖检测模块，普通设备可用）；
//                全程每帧检测码区，稳定命中即锁定并立即转入 FineSweep。
//   FineSweep    围绕最佳清晰度位置做窗口精扫；已锁定时先基于 ROI 测光
//                收敛曝光，再按 ROI 清晰度精调。
//   ProbeDark    精扫结束仍未锁定且设备具备检测能力时，把亮度压到暗区
//   ProbeBright  / 抬到亮区各驻留数帧，探测“更暗/更亮才可见”的码区；
//                探针期间一旦锁定，跳回 FineSweep 精调后直接进入曝光阶段。
//
// ExposurePerLightProfile：逐补光灯组合做传统 AE；锁定后按码区 ROI 测光，
// 焦点钉在 ROI 清晰度最优位置；检测持续运行，允许此阶段后补锁定。
//
// 状态复用约束（include/at_*.h 冻结，不得新增成员）：
//   state_.focus_tune_index    = 对焦子阶段编码（FocusStage）
//   state_.exposure_tune_index = 子阶段内步数计数（进入曝光阶段后恢复原义）
//   state_.light_profile_index = 精扫期间暂存精扫中心（center+1，0=未设置；
//                                进入曝光阶段时清零恢复原义）
//   previous_roi_              = 锁定的码区 ROI
//   探针驻留帧数、连续曝光调整数等从 candidates_ 尾部派生。
// ============================================================================

// ---- 码区锁定判据 ----
constexpr double kRoiLockIoU = 0.5;
constexpr int kRoiLockHits = 3;
// 锁定 ROI 与新检测框的平滑权重（新框占比）。
constexpr double kRoiBlend = 0.5;

// ---- 比例式 AE ----
// 亮度近似正比于 exp_time × gain：单步按 target/brightness 比例更新，
// 并限制单步最大变化倍率，避免测光噪声导致的震荡。
constexpr double kExposureRatioMax = 4.0;
constexpr double kExposureRatioMin = 0.25;
// 饱和（高光溢出）超限时强制回退的倍率上限。
constexpr double kSaturationBackoffRatio = 0.6;

// ---- 亮度探针 ----
// 暗探针目标 = kDarkProbeFactor × min_ready；
// 亮探针目标 = max_ready + kBrightProbeHeadroom × (255 - max_ready)。
constexpr double kDarkProbeFactor = 0.5;
constexpr double kBrightProbeHeadroom = 0.25;
// 探针亮度收敛步数上限与达标后的检测驻留帧数（给连续命中判据留窗口）。
constexpr int kProbeSettleCap = 3;
constexpr int kProbeDwellFrames = kRoiLockHits;
constexpr int kProbeMaxFrames = kProbeSettleCap + kProbeDwellFrames + 2;

// 对焦子阶段编码（存入 state_.focus_tune_index）。
enum class FocusStage : int {
    CoarseSweep = 0,
    FineSweep = 1,
    ProbeDark = 2,
    ProbeBright = 3,
};

FocusStage DecodeFocusStage(int encoded)
{
    if (encoded < 0 || encoded > static_cast<int>(FocusStage::ProbeBright)) {
        return FocusStage::CoarseSweep;
    }
    return static_cast<FocusStage>(encoded);
}

bool ParamsEqual(const CameraParams &lhs, const CameraParams &rhs)
{
    return lhs.exp_time == rhs.exp_time && lhs.exp_gain == rhs.exp_gain &&
           lhs.focus_pos == rhs.focus_pos && lhs.lights == rhs.lights;
}

bool IsExposureAction(TuneAction action)
{
    return action == TuneAction::IncreaseExposure || action == TuneAction::DecreaseExposure;
}

// 从候选记录尾部统计对焦阶段连续曝光调整次数（跨帧派生状态）。
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

// 从候选记录尾部统计对焦阶段连续 Hold（探针驻留）帧数。
int TrailingFocusHolds(const std::vector<CandidateRecord> &candidates)
{
    int count = 0;
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        if (it->phase == StepPhase::FocusTuneWithCoarseExposure && it->action == TuneAction::Hold) {
            ++count;
        } else {
            break;
        }
    }
    return count;
}

// 历史上是否出现过可用检测（探针只在设备具备检测能力时才值得做）。
bool AnyDetectionSeen(const std::vector<CandidateRecord> &candidates)
{
    for (const CandidateRecord &candidate : candidates) {
        if (candidate.heatmap.available) {
            return true;
        }
    }
    return false;
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

// 对焦阶段清晰度最优的候选帧；prefer_detected 时优先只看有检测的帧（ROI 清晰度）。
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

double ReadyBrightnessMid(const FlowConfig &flow)
{
    return 0.5 * (flow.min_ready_brightness + flow.max_ready_brightness);
}

int SettleBudget(const FlowConfig &flow)
{
    return std::max(1, flow.exposure_steps_per_profile);
}

// 比例式 AE 单步：按 target/brightness 计算曝光量（exp_time × gain）目标，
// 增亮优先加曝光时间（到顶后抬增益），减亮优先降增益（到底后降时间），
// 全程保持低增益优先以控制噪声。返回值可能与 current 相同（已被边界钳位）。
CameraParams StepExposureTowards(const CameraRange &camera,
                                 const CameraParams &current,
                                 double brightness,
                                 double target,
                                 double saturation_ratio,
                                 double max_saturation)
{
    double ratio = target / std::max(brightness, 1.0);
    ratio = std::clamp(ratio, kExposureRatioMin, kExposureRatioMax);
    if (saturation_ratio > max_saturation) {
        ratio = std::min(ratio, kSaturationBackoffRatio);
    }

    const int current_gain = std::max(current.exp_gain, 1);
    const double product =
        std::max(1.0, static_cast<double>(current.exp_time) * current_gain * ratio);

    CameraParams next = current;
    if (ratio >= 1.0) {
        next.exp_time = std::clamp(static_cast<int>(std::lround(product / current_gain)),
                                   camera.min_exp_time, camera.max_exp_time);
        next.exp_gain = std::clamp(static_cast<int>(std::lround(product / next.exp_time)),
                                   camera.min_exp_gain, camera.max_exp_gain);
    } else {
        next.exp_gain = std::clamp(static_cast<int>(std::lround(product / std::max(current.exp_time, 1))),
                                   camera.min_exp_gain, camera.max_exp_gain);
        next.exp_time = std::clamp(static_cast<int>(std::lround(product / next.exp_gain)),
                                   camera.min_exp_time, camera.max_exp_time);
    }
    return next;
}

// 曝光量（exp_time × gain）实际变化方向；饱和回退可能与亮度差方向相反。
TuneAction ExposureDirection(const CameraParams &current, const CameraParams &next)
{
    const long before = static_cast<long>(current.exp_time) * std::max(current.exp_gain, 1);
    const long after = static_cast<long>(next.exp_time) * std::max(next.exp_gain, 1);
    return after >= before ? TuneAction::IncreaseExposure : TuneAction::DecreaseExposure;
}

// 对焦阶段单步计划：settle/dwell/移动焦点三选一，note 追加到 decision.reason。
struct FocusPlan {
    TuneAction action = TuneAction::Hold;
    CameraParams next_params{};
    std::string note;
};

FocusPlan MakeSettlePlan(const SessionConfig &config,
                         const Observation &observation,
                         double target,
                         std::string note)
{
    FocusPlan plan;
    plan.next_params = StepExposureTowards(config.camera,
                                           observation.current_params,
                                           observation.quality.brightness,
                                           target,
                                           observation.quality.saturation_ratio,
                                           config.flow.max_ready_saturation);
    if (ParamsEqual(plan.next_params, observation.current_params)) {
        // 已被边界钳位，无法继续收敛：返回 Hold 交由调用方降级处理。
        plan.action = TuneAction::Hold;
        return plan;
    }
    plan.action = ExposureDirection(observation.current_params, plan.next_params);
    plan.note = std::move(note);
    return plan;
}

// 粗对焦：step==0 时先把亮度收敛到就绪窗口，再整遍等距扫描焦点行程。
FocusPlan PlanCoarseSweep(const SessionConfig &config,
                          int step,
                          const std::vector<CandidateRecord> &candidates,
                          const Observation &observation,
                          bool brightness_ready)
{
    if (step == 0 && !brightness_ready &&
        TrailingExposureTunes(candidates) < SettleBudget(config.flow)) {
        const double target = ReadyBrightnessMid(config.flow);
        FocusPlan plan = MakeSettlePlan(config, observation, target,
                                        "; precondition brightness target " +
                                            std::to_string(static_cast<int>(target)));
        if (IsExposureAction(plan.action)) {
            return plan;
        }
        // 曝光已到边界仍不达标：放弃收敛，直接开始扫描。
    }

    const int coarse_steps = std::max(1, config.flow.focus_tune_steps);
    const int slot = std::min(step, coarse_steps - 1);
    const int focus_range = config.camera.max_focus_pos - config.camera.min_focus_pos;
    FocusPlan plan;
    plan.action = TuneAction::AdjustFocus;
    plan.next_params = observation.current_params;
    plan.next_params.focus_pos =
        config.camera.min_focus_pos +
        static_cast<int>(std::lround((slot + 0.5) * focus_range /
                                     static_cast<double>(coarse_steps)));
    plan.note = "; coarse focus slot " + std::to_string(slot) + "/" + std::to_string(coarse_steps);
    return plan;
}

// 精对焦：围绕 center 在 [-half, +half] 窗口等距精扫；
// 已锁定时精扫开始前先基于 ROI 测光收敛曝光，整遍精扫不再调曝光。
FocusPlan PlanFineSweep(const SessionConfig &config,
                        int step,
                        int center,
                        const std::vector<CandidateRecord> &candidates,
                        const Observation &observation,
                        bool brightness_ready,
                        bool locked)
{
    if (step == 0 && locked && !brightness_ready &&
        TrailingExposureTunes(candidates) < SettleBudget(config.flow)) {
        FocusPlan plan = MakeSettlePlan(config, observation, ReadyBrightnessMid(config.flow),
                                        "; settle roi brightness before fine focus");
        if (IsExposureAction(plan.action)) {
            return plan;
        }
    }

    const int coarse_steps = std::max(1, config.flow.focus_tune_steps);
    const int fine_steps = std::max(2, coarse_steps);
    const int fine_index = std::min(step, fine_steps - 1);
    const int focus_range = config.camera.max_focus_pos - config.camera.min_focus_pos;
    const int half = std::max(config.budget.focus_step, focus_range / (2 * coarse_steps));
    const double offset =
        -half + fine_index * (2.0 * half) / static_cast<double>(fine_steps - 1);
    FocusPlan plan;
    plan.action = TuneAction::AdjustFocus;
    plan.next_params = observation.current_params;
    plan.next_params.focus_pos = center + static_cast<int>(std::lround(offset));
    plan.note = "; fine focus step " + std::to_string(fine_index) + "/" +
                std::to_string(fine_steps) + " center " + std::to_string(center);
    return plan;
}

double ProbeTargetBrightness(const FlowConfig &flow, bool dark)
{
    return dark ? kDarkProbeFactor * flow.min_ready_brightness
                : flow.max_ready_brightness +
                      kBrightProbeHeadroom * (255.0 - flow.max_ready_brightness);
}

// 亮度探针：先把亮度收敛到探针目标（暗区/亮区），随后驻留数帧给检测机会。
// 探针不做饱和回退（亮探针本就要求越过常规窗口）。
FocusPlan PlanDetectionProbe(const SessionConfig &config,
                             bool dark,
                             const std::vector<CandidateRecord> &candidates,
                             const Observation &observation)
{
    const char *label = dark ? "dark" : "bright";
    const double target = ProbeTargetBrightness(config.flow, dark);
    const double window = config.flow.max_ready_brightness - config.flow.min_ready_brightness;
    const double tolerance = std::max(10.0, 0.1 * window);

    if (std::abs(observation.quality.brightness - target) > tolerance &&
        TrailingExposureTunes(candidates) < kProbeSettleCap) {
        FocusPlan plan;
        plan.next_params = StepExposureTowards(config.camera,
                                               observation.current_params,
                                               observation.quality.brightness,
                                               target,
                                               /*saturation_ratio=*/0.0,
                                               /*max_saturation=*/1.0);
        if (!ParamsEqual(plan.next_params, observation.current_params)) {
            plan.action = ExposureDirection(observation.current_params, plan.next_params);
            plan.note = std::string("; probe ") + label + " settle target " +
                        std::to_string(static_cast<int>(target));
            return plan;
        }
    }

    FocusPlan plan;
    plan.action = TuneAction::Hold;
    plan.next_params = observation.current_params;
    plan.note = std::string("; probe ") + label + " dwell for detection";
    return plan;
}

// 探针完成判据：已驻留满 kProbeDwellFrames 帧，或总步数触达硬上限。
bool ProbeComplete(const std::vector<CandidateRecord> &candidates, int probe_step_count)
{
    return TrailingFocusHolds(candidates) >= kProbeDwellFrames ||
           probe_step_count >= kProbeMaxFrames;
}

// 解码排名阶段的帧数上限：门控迟迟不满足或宿主不响应解码请求时，
// 用完 decode 预算外加少量等待帧就放弃排名，避免烧尽全局步数预算。
constexpr int kRankingExtraFrames = 4;

int RankingFrameBudget(const StrategyBudget &budget)
{
    return std::max(1, budget.decode_budget) + kRankingExtraFrames;
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
                                                    decision.action,
                                                    config_.flow);
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
// state_.phase）；阶段与子阶段的转移统一由 AdvancePhaseAfterStep 负责。
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
            const bool locked = !previous_roi_.empty();
            const bool ready = BrightnessReady(observation.quality);
            const int step = state_.exposure_tune_index;

            FocusPlan plan;
            switch (DecodeFocusStage(state_.focus_tune_index)) {
                case FocusStage::CoarseSweep:
                    plan = PlanCoarseSweep(config_, step, candidates_, observation, ready);
                    break;
                case FocusStage::FineSweep: {
                    // 精扫中心在进入精扫时锁存（light_profile_index 暂存 center+1），
                    // 避免精扫途中“最佳清晰度帧”被自身覆盖导致中心漂移。
                    const int center = state_.light_profile_index > 0
                                           ? state_.light_profile_index - 1
                                           : observation.current_params.focus_pos;
                    plan = PlanFineSweep(config_, step, center, candidates_, observation,
                                         ready, locked);
                    break;
                }
                case FocusStage::ProbeDark:
                    plan = PlanDetectionProbe(config_, true, candidates_, observation);
                    break;
                case FocusStage::ProbeBright:
                    plan = PlanDetectionProbe(config_, false, candidates_, observation);
                    break;
            }
            decision.action = plan.action;
            decision.next_params = plan.next_params;
            decision.reason += plan.note;
            break;
        }
        case StepPhase::ExposurePerLightProfile: {
            // 曝光阶段检测持续运行：对焦阶段未找到码区时仍可在此后补锁定，
            // 锁定后 BuildObservation 自动切换为 ROI 测光。
            if (config_.flow.enable_heatmap &&
                UpdateRoiLock(observation, candidates_, previous_roi_)) {
                decision.reason += "; roi locked";
            }
            const auto &profiles = config_.flow.light_profiles;
            if (state_.light_profile_index < static_cast<int>(profiles.size())) {
                decision.action = TuneAction::AdjustLight;
                decision.next_params = observation.current_params;
                decision.next_params.lights = profiles[state_.light_profile_index];
                // 焦点钉在对焦阶段的最佳清晰度位置（锁定时为 ROI 清晰度最优）。
                if (const CandidateRecord *best =
                        BestFocusCandidate(candidates_, !previous_roi_.empty())) {
                    decision.next_params.focus_pos = best->params.focus_pos;
                }
                if (!BrightnessReady(observation.quality)) {
                    const double target = ReadyBrightnessMid(config_.flow);
                    const CameraParams tuned =
                        StepExposureTowards(config_.camera, decision.next_params,
                                            observation.quality.brightness, target,
                                            observation.quality.saturation_ratio,
                                            config_.flow.max_ready_saturation);
                    if (!ParamsEqual(tuned, decision.next_params)) {
                        decision.action = ExposureDirection(decision.next_params, tuned);
                        decision.next_params = tuned;
                    }
                }
                decision.reason += "; light profile " + std::to_string(state_.light_profile_index);
            } else {
                decision.action = TuneAction::Finish;
                decision.reason += "; all light profiles done";
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
            // 排名阶段帧数保险丝（exposure_tune_index 在此阶段复用为帧计数）。
            if (state_.exposure_tune_index >= RankingFrameBudget(config_.budget)) {
                state_.ranking_complete = true;
                decision.action = TuneAction::Hold;
                decision.reason = "ranking frame budget exhausted";
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
    return ParamsEqual(lhs, rhs);
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

// 固定步长的保底调参：仅用于 no-op 升级与防御路径；
// 常规曝光收敛使用比例式 StepExposureTowards。
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

    switch (state_.phase) {
        case StepPhase::FocusTuneWithCoarseExposure: {
            const FocusStage stage = DecodeFocusStage(state_.focus_tune_index);
            const bool locked = !previous_roi_.empty();
            const int coarse_steps = std::max(1, config_.flow.focus_tune_steps);
            const int fine_steps = std::max(2, coarse_steps);

            const auto enter_fine_sweep = [this](bool prefer_detected, int fallback_focus) {
                const CandidateRecord *best = BestFocusCandidate(candidates_, prefer_detected);
                const int center = best ? best->params.focus_pos : fallback_focus;
                state_.focus_tune_index = static_cast<int>(FocusStage::FineSweep);
                state_.exposure_tune_index = 0;
                state_.light_profile_index = center + 1; // 精扫中心锁存（0=未设置）
            };
            const auto enter_exposure_phase = [this] {
                state_.phase = StepPhase::ExposurePerLightProfile;
                state_.focus_tune_index = 0;
                state_.exposure_tune_index = 0;
                state_.light_profile_index = 0;
            };
            const auto enter_probe = [this](FocusStage probe) {
                state_.focus_tune_index = static_cast<int>(probe);
                state_.exposure_tune_index = 0;
                state_.light_profile_index = 0;
            };
            // 探针只在设备具备检测能力时才值得做：配置了检测模型，
            // 或者宿主注入过可用检测观测。
            const auto probes_enabled = [this] {
                return config_.flow.enable_heatmap &&
                       (!config_.heatmap.model_path.empty() || AnyDetectionSeen(candidates_));
            };

            // 1) 子阶段内步数计入：扫描阶段只数焦点移动，探针阶段每帧都计。
            if (stage == FocusStage::CoarseSweep || stage == FocusStage::FineSweep) {
                if (decision.action == TuneAction::AdjustFocus) {
                    state_.exposure_tune_index += 1;
                }
            } else {
                state_.exposure_tune_index += 1;
            }

            // 2) 锁定跳转：粗扫/探针期间锁定即转入 ROI 精扫（“立即精调”）；
            //    精扫途中锁定则继续当前精扫，测光已自动切到 ROI。
            if (locked && stage != FocusStage::FineSweep) {
                enter_fine_sweep(/*prefer_detected=*/true, decision.next_params.focus_pos);
                break;
            }

            // 3) 子阶段完成判定
            switch (stage) {
                case FocusStage::CoarseSweep:
                    if (state_.exposure_tune_index >= coarse_steps) {
                        enter_fine_sweep(/*prefer_detected=*/false, decision.next_params.focus_pos);
                    }
                    break;
                case FocusStage::FineSweep:
                    if (state_.exposure_tune_index >= fine_steps) {
                        if (locked || !probes_enabled()) {
                            enter_exposure_phase();
                        } else {
                            enter_probe(FocusStage::ProbeDark);
                        }
                    }
                    break;
                case FocusStage::ProbeDark:
                    if (ProbeComplete(candidates_, state_.exposure_tune_index)) {
                        enter_probe(FocusStage::ProbeBright);
                    }
                    break;
                case FocusStage::ProbeBright:
                    if (ProbeComplete(candidates_, state_.exposure_tune_index)) {
                        enter_exposure_phase();
                    }
                    break;
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
            state_.exposure_tune_index += 1;
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
    // 对焦阶段：动作被边界钳位时以焦点微动脱困（扫描计数依赖 AdjustFocus 推进）。
    // 其余阶段：改为 Hold 原地等待阶段推进，避免无意义的镜头移动。
    if (state_.phase == StepPhase::FocusTuneWithCoarseExposure) {
        decision.reason += "; no-op escalation";
        decision.action = TuneAction::AdjustFocus;
        decision.next_params = ApplyAction(decision.next_params, TuneAction::AdjustFocus, observation);
    } else {
        decision.reason += "; no-op hold";
        decision.action = TuneAction::Hold;
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
