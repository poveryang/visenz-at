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
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(20);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(!result.finished);
    assert(result.trace.difficulty == at::SceneDifficulty::Hard);
    assert(result.trace.action == at::TuneAction::IncreaseExposure);
    assert(result.next_params.exp_time > input.current_params.exp_time);
}

void TestBrightFrameDecreasesGain()
{
    at::ATSession session(MakeConfig());
    at::FrameInput input;
    input.image = MakeImage(245);
    input.current_params = MakeParams();

    const at::StepResult result = session.Step(input);
    assert(!result.finished);
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
    assert(result.trace.action == at::TuneAction::RequestDecode);
    assert(result.best_candidate.has_value());
    assert(session.Candidates().size() == 1);
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
    assert(result.best_candidate.has_value());
    assert(result.best_candidate->decode.success);
    assert(result.trace.difficulty == at::SceneDifficulty::Simple);
}

} // namespace

int main()
{
    TestDarkFrameIncreasesExposure();
    TestBrightFrameDecreasesGain();
    TestHeatmapCanRequestDecode();
    TestDecodeSuccessFinishesWithCandidate();
    std::cout << "at_core_unit_test passed" << std::endl;
    return 0;
}
