#include "at_orchestrator.h"
#include "at_session.h"
#include "at_types.h"

#include <cassert>
#include <iostream>

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

void TestFocusSweepAdvancesPhase()
{
    at::AtSession session(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult result;
    for (int i = 0; i < MakeConfig().flow.focus_tune_steps + 1; ++i) {
        result = session.ProcessStep(context);
        context.current_params = result.next_params;
    }
    assert(result.trace.phase == at::StepPhase::ExposurePerLightProfile);
}

void TestPipelineFinishesWithoutDecode()
{
    at::AtOrchestrator orchestrator(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();

    at::StepResult last;
    for (int i = 0; i < 60 && !last.finished; ++i) {
        last = orchestrator.ProcessStep(context);
        context.current_params = last.next_params;
        context.decode = {};
    }
    assert(last.finished);
    assert(last.trace.trace_version == at::kTraceVersion);
    assert(last.trace.finish_reason == at::FinishReason::BestCandidateFound ||
           last.trace.finish_reason == at::FinishReason::BudgetExhausted);
}

void TestGetLibVersionMatchesProject()
{
    assert(std::string(at::GetLibVersion()) == "1.0.0");
}

void TestTraceVersionPresent()
{
    at::AtSession session(MakeConfig());
    at::FrameContext context;
    context.image = MakeImage(120);
    context.current_params = MakeParams();
    const at::StepResult result = session.ProcessStep(context);
    assert(result.trace.trace_version == at::kTraceVersion);
}

} // namespace

int main()
{
    TestGetLibVersionMatchesProject();
    TestDarkFrameIncreasesExposure();
    TestFocusSweepAdvancesPhase();
    TestPipelineFinishesWithoutDecode();
    TestTraceVersionPresent();
    std::cout << "at_core_unit_test: all passed\n";
    return 0;
}
