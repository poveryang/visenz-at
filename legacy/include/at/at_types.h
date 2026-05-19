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
    BrightnessPrecondition,
    RoiObserve,
    FocusExplore,
    LocalRefine,
    DecodeVerify,
    PostDecodeRefine,
    FallbackSearch,
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

enum class FinishReason {
    None,
    DecodeSuccess,
    QualityStable,
    BestCandidateFound,
    BudgetExhausted,
    SearchFailed,
    ExternalStepLimit,
    Error,
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

struct Observation {
    CameraParams current_params{};
    cv::Rect roi{};
    ImageQuality quality{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
    SceneDifficulty difficulty = SceneDifficulty::Normal;
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

struct FlowConfig {
    std::vector<StepPhase> stages{
        StepPhase::BrightnessPrecondition,
        StepPhase::FocusExplore,
        StepPhase::LocalRefine,
        StepPhase::DecodeVerify,
    };
    int focus_explore_steps = 8;
    int local_refine_steps = 2;
    int decode_interval_steps = 8;
    double min_ready_brightness = 55.0;
    double max_ready_brightness = 190.0;
    double max_ready_saturation = 0.12;
    bool enable_roi_observe = false;
    bool enable_periodic_decode = false;
    bool enable_heatmap_decode = true;
};

struct SessionConfig {
    CameraRange camera{};
    StrategyBudget simple{24, 2, 300, 8, 6, false, true};
    StrategyBudget normal{48, 6, 600, 16, 8, true, true};
    StrategyBudget hard{96, 12, 900, 24, 12, true, true};
    FlowConfig flow{};
};

struct SessionState {
    int step_index = 0;
    int stage_step_count = 0;
    int decode_used = 0;
    StepPhase phase = StepPhase::Observe;
    SceneDifficulty historical_max_difficulty = SceneDifficulty::Simple;
    FinishReason finish_reason = FinishReason::None;
};

struct TuningDecision {
    StepPhase phase = StepPhase::Observe;
    TuneAction action = TuneAction::Hold;
    CameraParams next_params{};
    bool need_decode = false;
    bool finished = false;
    FinishReason finish_reason = FinishReason::None;
    std::string reason;
};

std::string ToString(SceneDifficulty difficulty);
std::string ToString(StepPhase phase);
std::string ToString(TuneAction action);
std::string ToString(FinishReason reason);

} // namespace at

#endif // AT_AT_TYPES_H
