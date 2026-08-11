#include "at_orchestrator.h"
#include "at_session.h"
#include "at_types.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

at::SessionConfig MakeConfig()
{
    at::SessionConfig config;
    config.camera.min_exp_time = 100;
    config.camera.max_exp_time = 5000;
    config.camera.min_exp_gain = 1;
    config.camera.max_exp_gain = 128;
    config.camera.min_focus_pos = 0;
    config.camera.max_focus_pos = 100;
    config.budget.max_steps = 80;
    config.budget.focus_step = 5;
    config.budget.exposure_step = 300;
    config.flow.focus_tune_steps = 3;
    config.flow.exposure_steps_per_profile = 2;
    config.flow.light_profiles = {{0, 0, 0, 0}, {1, 0, 0, 0}};
    config.flow.enable_decode_ranking = false;
    return config;
}

at::CameraParams MakeParams()
{
    at::CameraParams params;
    params.exp_time = 1000;
    params.exp_gain = 32;
    params.lights = {0, 0, 0, 0};
    params.focus_pos = 40;
    return params;
}

cv::Mat MakeImage(int value)
{
    cv::Mat image(96, 128, CV_8UC1, cv::Scalar(value));
    cv::rectangle(image, cv::Rect(40, 28, 44, 32), cv::Scalar(255 - value), 2);
    return image;
}

at::HeatmapObservation MakeDetection(const cv::Rect &roi, double confidence)
{
    at::HeatmapObservation observation;
    observation.available = true;
    observation.roi = roi;
    observation.confidence = confidence;
    observation.source = "test";
    return observation;
}

bool ReasonSeen(const std::vector<std::string> &reasons, const std::string &needle)
{
    for (const std::string &reason : reasons) {
        if (reason.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 暗帧首步应先收敛亮度（对焦前的亮度预处理），且比例式 AE 一步显著提升曝光量
void TestDarkFrameIncreasesExposure()
{
    at::AtSession session(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(20);
    context.current_params = MakeParams();

    const at::StepResult result = session.ProcessStep(context);
    assert(!result.finished);
    assert(result.trace.phase == at::StepPhase::FocusTuneWithCoarseExposure);
    assert(result.trace.action == at::TuneAction::IncreaseExposure);
    // 比例式更新：一步至少放大 2x（目标亮度/当前亮度 >> 上限 4x）
    assert(result.next_params.exp_time >= context.current_params.exp_time * 2);
}

// 过亮帧首步应下调曝光
void TestBrightFrameDecreasesExposure()
{
    at::AtSession session(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(245);
    context.current_params = MakeParams();

    const at::StepResult result = session.ProcessStep(context);
    assert(result.trace.action == at::TuneAction::DecreaseExposure);
    const long before = static_cast<long>(context.current_params.exp_time) *
                        context.current_params.exp_gain;
    const long after = static_cast<long>(result.next_params.exp_time) *
                       result.next_params.exp_gain;
    assert(after < before);
}

// 关闭检测：粗扫 + 精扫后直接进入曝光阶段，无探针
void TestFocusSweepAdvancesPhaseWithoutDetect()
{
    at::SessionConfig config = MakeConfig();
    config.flow.enable_heatmap = false;
    at::AtSession session(config);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    const int coarse = config.flow.focus_tune_steps;
    const int fine = std::max(2, coarse);
    at::StepResult result;
    std::vector<std::string> reasons;
    for (int i = 0; i < coarse + fine + 1; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        reasons.push_back(result.trace.reason);
        if (result.trace.phase != at::StepPhase::FocusTuneWithCoarseExposure) {
            break;
        }
    }
    assert(ReasonSeen(reasons, "direction probe") || ReasonSeen(reasons, "coarse primary") ||
           ReasonSeen(reasons, "coarse opposite"));
    assert(ReasonSeen(reasons, "fine focus"));
    assert(!ReasonSeen(reasons, "probe"));
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 未配置检测模型且从未见过检测观测：不做亮度探针
void TestNoProbesWithoutDetectionCapability()
{
    const at::SessionConfig config = MakeConfig();
    at::AtSession session(config);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    std::vector<std::string> reasons;
    for (int i = 0; i < 40; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        reasons.push_back(result.trace.reason);
        if (result.trace.phase != at::StepPhase::FocusTuneWithCoarseExposure) {
            break;
        }
    }
    assert(!ReasonSeen(reasons, "probe"));
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 配置了检测模型但目标始终未出现：精扫后做暗/亮两次探针再进入曝光阶段
void TestProbesRunWhenModelConfigured()
{
    at::SessionConfig config = MakeConfig();
    config.heatmap.model_path = "stub-model";
    at::AtSession session(config);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    std::vector<std::string> reasons;
    for (int i = 0; i < 60; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        reasons.push_back(result.trace.reason);
        if (result.trace.phase != at::StepPhase::FocusTuneWithCoarseExposure) {
            break;
        }
    }
    assert(ReasonSeen(reasons, "probe dark"));
    assert(ReasonSeen(reasons, "probe bright"));
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 粗扫期间稳定检测到码区：立即锁定并转入 ROI 精扫
void TestLockDuringCoarseTriggersImmediateFine()
{
    const at::SessionConfig config = MakeConfig();
    at::AtSession session(config);
    const cv::Rect code_roi(40, 28, 44, 32);

    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    bool saw_lock = false;
    bool saw_fine_focus = false;
    at::StepResult result;
    for (int i = 0; i < 20; ++i) {
        context.heatmap = MakeDetection(code_roi, 0.9);
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        if (result.trace.reason.find("roi locked") != std::string::npos) {
            saw_lock = true;
        }
        if (saw_lock && result.trace.reason.find("fine focus") != std::string::npos) {
            saw_fine_focus = true;
        }
        if (result.trace.phase != at::StepPhase::FocusTuneWithCoarseExposure) {
            break;
        }
    }
    assert(saw_lock);
    assert(saw_fine_focus);
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);

    // 锁定后候选帧的质量统计区域应为检测 ROI
    bool roi_metered = false;
    for (const at::CandidateRecord &candidate : session.Candidates()) {
        if (candidate.roi == code_roi) {
            roi_metered = true;
            break;
        }
    }
    assert(roi_metered);
}

// 探针期间发现码区：跳回 ROI 精扫，跳过剩余探针
void TestLockDuringProbeReturnsToFineFocus()
{
    at::SessionConfig config = MakeConfig();
    config.heatmap.model_path = "stub-model";
    at::AtSession session(config);
    const cv::Rect code_roi(40, 28, 44, 32);

    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    bool probe_dark_started = false;
    bool saw_lock = false;
    bool fine_after_lock = false;
    bool saw_probe_bright = false;
    at::StepResult result;
    for (int i = 0; i < 60; ++i) {
        context.heatmap = probe_dark_started ? MakeDetection(code_roi, 0.9)
                                             : at::HeatmapObservation{};
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        if (result.trace.reason.find("probe dark") != std::string::npos) {
            probe_dark_started = true;
        }
        if (result.trace.reason.find("probe bright") != std::string::npos) {
            saw_probe_bright = true;
        }
        if (result.trace.reason.find("roi locked") != std::string::npos) {
            saw_lock = true;
        }
        if (saw_lock && result.trace.reason.find("fine focus") != std::string::npos) {
            fine_after_lock = true;
        }
        if (result.trace.phase != at::StepPhase::FocusTuneWithCoarseExposure) {
            break;
        }
    }
    assert(probe_dark_started);
    assert(saw_lock);
    assert(fine_after_lock);
    assert(!saw_probe_bright);
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 锁定后检测偶发丢失：仍沿用锁定 ROI 测光，不回退全图
void TestLockedRoiSurvivesDetectionDropout()
{
    const at::SessionConfig config = MakeConfig();
    at::AtSession session(config);
    const cv::Rect code_roi(40, 28, 44, 32);

    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    for (int i = 0; i < 4; ++i) {
        context.heatmap = MakeDetection(code_roi, 0.9);
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
    }
    // 丢失一帧检测
    context.heatmap = {};
    result = session.ProcessStep(context);
    const at::CandidateRecord &last = session.Candidates().back();
    assert(!last.heatmap.available);
    assert(!last.roi.empty());
    assert(last.roi != cv::Rect(0, 0, context.image.cols, context.image.rows));
}

void TestPipelineFinishesWithoutDecode()
{
    at::AtOrchestrator orchestrator(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult last;
    for (int i = 0; i < 80 && !last.finished; ++i) {
        last = orchestrator.ProcessStep(context);
        context.current_params = last.next_params;
        context.decode = {};
        context.heatmap = {};
    }
    assert(last.finished);
    assert(last.trace.finish_reason == at::FinishReason::BestCandidateFound ||
           last.trace.finish_reason == at::FinishReason::BudgetExhausted);
}

// 带稳定检测的完整流程：应正常完成且最优候选带有检测观测
void TestPipelineFinishesWithDetection()
{
    at::AtOrchestrator orchestrator(MakeConfig());
    const cv::Rect code_roi(40, 28, 44, 32);

    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult last;
    for (int i = 0; i < 80 && !last.finished; ++i) {
        context.heatmap = MakeDetection(code_roi, 0.9);
        context.decode = {};
        last = orchestrator.ProcessStep(context);
        context.current_params = last.next_params;
    }
    assert(last.finished);
    assert(last.best_candidate.has_value());
    assert(last.best_candidate->heatmap.available);
}

// 低 YOLO 置信度（常见 ~0.27）不应在 ranking 空转 Hold；应请求解码或软门控放行。
void TestDecodeRankingRequestsAtLowConfidence()
{
    at::SessionConfig config = MakeConfig();
    config.flow.enable_decode_ranking = true;
    config.flow.enable_heatmap = true;
    config.flow.light_profiles = {{0, 0, 0, 0}};
    config.flow.focus_tune_steps = 2;
    config.flow.exposure_steps_per_profile = 1;
    config.budget.decode_budget = 4;
    config.heatmap.threshold = 0.25;
    config.heatmap.model_path = "stub-model";

    at::AtSession session(config);
    const cv::Rect code_roi(40, 28, 44, 32);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    bool saw_decode_request = false;
    bool saw_waiting_only = true;
    at::StepResult last;
    for (int i = 0; i < 80 && !last.finished; ++i) {
        context.heatmap = MakeDetection(code_roi, 0.27);
        context.decode = {};
        last = session.ProcessStep(context);
        context.current_params = last.next_params;
        if (last.trace.phase == at::StepPhase::CandidateDecodeRanking) {
            if (last.need_decode) {
                saw_decode_request = true;
                saw_waiting_only = false;
                break;
            }
            if (last.trace.reason.find("waiting decode gate") == std::string::npos) {
                saw_waiting_only = false;
            }
        }
    }
    assert(saw_decode_request);
    assert(!saw_waiting_only);
}

void TestAbortWritesInterruptedEnd()
{
    at::SessionConfig config = MakeConfig();
    config.run_log.enable = true;
    config.run_log.dir = "/tmp/visenz_at_unit_abort";
    at::AtOrchestrator orchestrator(config);
    orchestrator.Reset();

    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();
    (void)orchestrator.ProcessStep(context);

    orchestrator.Abort();
    // 再次 Abort 应幂等，不抛异常。
    orchestrator.Abort();
}

void TestGetLibVersionMatchesProject()
{
#ifdef AT_VERSION_STR
    assert(std::string(at::GetLibVersion()) == AT_VERSION_STR);
#else
    assert(!std::string(at::GetLibVersion()).empty());
#endif
}

} // namespace

int main()
{
    TestGetLibVersionMatchesProject();
    TestDarkFrameIncreasesExposure();
    TestBrightFrameDecreasesExposure();
    TestFocusSweepAdvancesPhaseWithoutDetect();
    TestNoProbesWithoutDetectionCapability();
    TestProbesRunWhenModelConfigured();
    TestLockDuringCoarseTriggersImmediateFine();
    TestLockDuringProbeReturnsToFineFocus();
    TestLockedRoiSurvivesDetectionDropout();
    TestPipelineFinishesWithoutDecode();
    TestPipelineFinishesWithDetection();
    TestDecodeRankingRequestsAtLowConfidence();
    TestAbortWritesInterruptedEnd();
    std::cout << "at_core_unit_test: all passed\n";
    return 0;
}
