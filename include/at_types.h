#ifndef AT_AT_TYPES_H
#define AT_AT_TYPES_H

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace at {

inline constexpr double kDefaultHeatmapThreshold = 40.0;

// 库发布版本：在 CMakeLists.txt 的 project(AT VERSION ...) 中维护
const char *GetLibVersion();

enum class StepPhase {
    Observe,
    FocusTuneWithCoarseExposure,
    ExposurePerLightProfile,
    CandidateDecodeRanking,
    SelectBest,
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
    NoOpEscalation,
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
    std::string model_version;
};

struct DecodeFeedback {
    bool attempted = false;
    bool success = false;
    double score = 0.0;
    std::vector<cv::Rect> regions;
    int latency_ms = 0;
    std::string summary;
};

struct PreprocessReport {
    std::vector<std::string> applied_ops;
    double quality_delta = 0.0;
};

struct FrameContext {
    cv::Mat image;
    CameraParams current_params{};
    cv::Rect manual_roi{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
    PreprocessReport preprocess{};
};

struct Observation {
    CameraParams current_params{};
    cv::Rect roi{};
    ImageQuality quality{};
    HeatmapObservation heatmap{};
    DecodeFeedback decode{};
    PreprocessReport preprocess{};
};

struct StrategyBudget {
    int max_steps = 48;
    int decode_budget = 6;
    int exposure_step = 500;
    int gain_step = 16;
    int focus_step = 8;
};

struct FlowConfig {
    int focus_tune_steps = 8;
    int exposure_steps_per_profile = 6;
    double min_ready_brightness = 55.0;
    double max_ready_brightness = 190.0;
    double max_ready_saturation = 0.12;
    bool enable_decode_ranking = false;
    bool enable_heatmap = true;
    std::vector<std::vector<int>> light_profiles{
        {0, 0, 0, 0},
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {1, 1, 0, 0},
    };
};

struct HeatmapConfig {
    std::string model_path;
    std::string context = "timvx";
    std::string precision = "uint8";
    double threshold = kDefaultHeatmapThreshold;
    bool overlay = true;
};

struct SessionConfig {
    CameraRange camera{};
    StrategyBudget budget{};
    FlowConfig flow{};
    HeatmapConfig heatmap{};
};

struct SessionState {
    int step_index = 0;
    int stage_step_count = 0;
    int decode_used = 0;
    int focus_tune_index = 0;
    int light_profile_index = 0;
    int exposure_tune_index = 0;
    StepPhase phase = StepPhase::Observe;
    FinishReason finish_reason = FinishReason::None;
    bool ranking_complete = false;
};

struct StepDecision {
    StepPhase phase = StepPhase::Observe;
    TuneAction action = TuneAction::Hold;
    CameraParams next_params{};
    bool need_decode = false;
    bool finished = false;
    FinishReason finish_reason = FinishReason::None;
    std::string reason;
};

std::string ToString(StepPhase phase);
std::string ToString(TuneAction action);
std::string ToString(FinishReason reason);

} // namespace at

#endif // AT_AT_TYPES_H
