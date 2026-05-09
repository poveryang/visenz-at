#ifndef AT_AT_TYPES_H
#define AT_AT_TYPES_H

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace at {

enum class SceneDifficulty {
    Simple,
    Normal,
    Hard,
};

enum class StepPhase {
    Observe,
    Tune,
    DecodeVerify,
    Refine,
    Done,
};

enum class TuneAction {
    Hold,
    IncreaseExposure,
    DecreaseExposure,
    AdjustLight,
    AdjustFocus,
    RequestDecode,
    Finish,
};

struct CameraRange {
    int min_exp_time = 1;
    int max_exp_time = 40000;
    int min_exp_gain = 1;
    int max_exp_gain = 128;
    int min_light = 0;
    int max_light = 1;
    int min_focus_pos = 0;
    int max_focus_pos = 1023;
};

struct CameraParams {
    int exp_time = 1000;
    int exp_gain = 32;
    std::vector<int> lights;
    int focus_pos = 0;
};

struct ImageQuality {
    double brightness = 0.0;
    double contrast = 0.0;
    double sharpness = 0.0;
    double saturation_ratio = 0.0;
    double noise_proxy = 0.0;
};

struct HeatmapObservation {
    bool available = false;
    cv::Rect roi{};
    double confidence = 0.0;
    double feature_strength = 0.0;
    double stability = 0.0;
    std::string source = "none";
};

struct DecodeFeedback {
    bool attempted = false;
    bool success = false;
    std::vector<cv::Rect> regions;
    std::string summary;
};

struct FrameInput {
    cv::Mat image;
    CameraParams current_params{};
    cv::Rect manual_roi{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
};

struct StrategyBudget {
    int max_steps = 48;
    int decode_budget = 6;
    int exposure_step = 500;
    int gain_step = 16;
    int focus_step = 8;
    bool allow_search_expansion = true;
    bool allow_post_decode_refine = true;
};

struct SessionConfig {
    CameraRange camera{};
    StrategyBudget simple{24, 2, 300, 8, 6, false, true};
    StrategyBudget normal{48, 6, 600, 16, 8, true, true};
    StrategyBudget hard{96, 12, 900, 24, 12, true, true};
};

std::string ToString(SceneDifficulty difficulty);
std::string ToString(StepPhase phase);
std::string ToString(TuneAction action);

} // namespace at

#endif // AT_AT_TYPES_H
