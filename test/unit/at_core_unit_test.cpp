#include "at_orchestrator.h"
#include "at_session.h"
#include "at_types.h"

#include <cassert>
#include <iostream>
#include <string>

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
    assert(result.next_params.exp_time > context.current_params.exp_time);
}

// 关闭检测时保持单遍粗对焦：focus_tune_steps 步后进入曝光阶段
void TestFocusSweepAdvancesPhaseWithoutDetect()
{
    at::SessionConfig config = MakeConfig();
    config.flow.enable_heatmap = false;
    at::AtSession session(config);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    for (int i = 0; i < config.flow.focus_tune_steps + 1; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
    }
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 开启检测但始终无码区：多亮度探测会进行多遍粗对焦，之后仍能正常收敛
void TestNoDetectionRunsBrightnessProbes()
{
    const at::SessionConfig config = MakeConfig();
    at::AtSession session(config);
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    int steps_in_focus_phase = 0;
    for (int i = 0; i < 60; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
        if (result.trace.phase == at::StepPhase::FocusTuneWithCoarseExposure) {
            ++steps_in_focus_phase;
        } else {
            break;
        }
    }
    // 至少两遍（> 单遍 focus_tune_steps），说明发生了多亮度探测
    assert(steps_in_focus_phase > config.flow.focus_tune_steps);
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

// 连续稳定检测到码区：锁定 ROI、进入精细对焦，并以 ROI 作为质量统计区域
void TestRoiLockTriggersFineFocus()
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
        if (result.trace.reason.find("fine focus") != std::string::npos) {
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
    TestFocusSweepAdvancesPhaseWithoutDetect();
    TestNoDetectionRunsBrightnessProbes();
    TestRoiLockTriggersFineFocus();
    TestLockedRoiSurvivesDetectionDropout();
    TestPipelineFinishesWithoutDecode();
    TestPipelineFinishesWithDetection();
    std::cout << "at_core_unit_test: all passed\n";
    return 0;
}
