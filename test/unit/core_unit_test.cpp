#include "at/at_session.h"

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
    config.camera.min_light = 0;
    config.camera.max_light = 1;
    config.camera.min_focus_pos = 0;
    config.camera.max_focus_pos = 100;
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
    const at::SessionConfig config = MakeConfig();
    at::ATSession session(config);
    at::FrameInput input;
    input.image = MakeImage(20);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(!result.finished);
    assert(result.trace.difficulty == at::SceneDifficulty::Hard);
    assert(result.trace.phase == at::StepPhase::BrightnessPrecondition);
    assert(result.trace.action == at::TuneAction::IncreaseExposure);
    assert(result.next_params.exp_time > input.current_params.exp_time);
    assert(result.next_params.exp_time > input.current_params.exp_time + config.hard.exposure_step);
}

void TestBrightFrameDecreasesGain()
{
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(245);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(!result.finished);
    assert(result.trace.phase == at::StepPhase::BrightnessPrecondition);
    assert(result.trace.action == at::TuneAction::DecreaseExposure);
    assert(result.next_params.exp_gain < input.current_params.exp_gain);
}

void TestHeatmapCanRequestDecode()
{
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(120);
    input.current_params = MakeParams();
    input.heatmap.available = true;
    input.heatmap.confidence = 0.85;
    input.heatmap.roi = cv::Rect(36, 24, 54, 42);

    const at::StepResult result = session.Step(input);
    assert(result.need_decode);
    assert(result.trace.phase == at::StepPhase::DecodeVerify);
    assert(result.trace.action == at::TuneAction::RequestDecode);
    assert(result.best_candidate.has_value());
    assert(session.Candidates().size() == 1);
}

void TestReadyFrameEntersFocusExploreWithoutHeatmap()
{
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(120);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(!result.need_decode);
    assert(!result.finished);
    assert(result.trace.phase == at::StepPhase::FocusExplore);
    assert(result.trace.action == at::TuneAction::AdjustFocus);
    assert(result.next_params.focus_pos > input.current_params.focus_pos);
}

void TestDefaultFlowCanFinishAfterConfiguredStages()
{
    at::SessionConfig config = MakeConfig();
    config.flow.focus_explore_steps = 2;
    config.flow.local_refine_steps = 1;

    at::ATSession session(config);
    at::FrameInput input;
    input.image = MakeImage(120);
    input.current_params = MakeParams();

    const at::StepResult first = session.Step(input);
    assert(first.trace.phase == at::StepPhase::FocusExplore);

    input.current_params = first.next_params;
    const at::StepResult second = session.Step(input);
    assert(second.trace.phase == at::StepPhase::FocusExplore);

    input.current_params = second.next_params;
    const at::StepResult third = session.Step(input);
    assert(third.trace.phase == at::StepPhase::LocalRefine);
    assert(third.trace.action == at::TuneAction::Hold);

    input.current_params = third.next_params;
    const at::StepResult fourth = session.Step(input);
    assert(fourth.finished);
    assert(fourth.trace.phase == at::StepPhase::Done);
    assert(fourth.trace.finish_reason == at::FinishReason::BestCandidateFound);
}

void TestDecodeSuccessFinishesWithCandidate()
{
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(110);
    input.current_params = MakeParams();
    input.decode.attempted = true;
    input.decode.success = true;
    input.decode.regions = {cv::Rect(36, 24, 54, 42)};

    const at::StepResult result = session.Step(input);
    assert(result.finished);
    assert(result.trace.finish_reason == at::FinishReason::DecodeSuccess);
    assert(result.best_candidate.has_value());
    assert(result.best_candidate->decode.success);
    assert(result.trace.difficulty == at::SceneDifficulty::Simple);
}

void TestHardHistoryKeepsHardBudget()
{
    at::SessionConfig config = MakeConfig();
    config.normal.max_steps = 2;
    config.hard.max_steps = 10;

    at::ATSession session(config);

    at::FrameInput dark;
    dark.image = MakeImage(20);
    dark.current_params = MakeParams();
    const at::StepResult first = session.Step(dark);
    assert(!first.finished);
    assert(first.trace.historical_max_difficulty == at::SceneDifficulty::Hard);
    assert(first.trace.active_max_steps == config.hard.max_steps);

    at::FrameInput normal;
    normal.image = MakeImage(120);
    normal.current_params = first.next_params;
    const at::StepResult second = session.Step(normal);
    assert(!second.finished);
    assert(second.trace.difficulty != at::SceneDifficulty::Hard);
    assert(second.trace.historical_max_difficulty == at::SceneDifficulty::Hard);
    assert(second.trace.active_max_steps == config.hard.max_steps);
}

void TestBudgetExhaustionReportsFinishReason()
{
    at::SessionConfig config = MakeConfig();
    config.hard.max_steps = 1;

    at::ATSession session(config);
    at::FrameInput input;
    input.image = MakeImage(20);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(result.finished);
    assert(result.trace.phase == at::StepPhase::Done);
    assert(result.trace.action == at::TuneAction::Finish);
    assert(result.trace.finish_reason == at::FinishReason::BudgetExhausted);
}

} // namespace

int main()
{
    TestDarkFrameIncreasesExposure();
    TestBrightFrameDecreasesGain();
    TestHeatmapCanRequestDecode();
    TestReadyFrameEntersFocusExploreWithoutHeatmap();
    TestDefaultFlowCanFinishAfterConfiguredStages();
    TestDecodeSuccessFinishesWithCandidate();
    TestHardHistoryKeepsHardBudget();
    TestBudgetExhaustionReportsFinishReason();
    std::cout << "at_core_unit_test passed" << std::endl;
    return 0;
}
