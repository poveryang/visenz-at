#include "at_interface.h"

#include "at_orchestrator.h"
#include "at_run_log.h"
#include "compat/barcode_decode_provider.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace at {
namespace {

CameraParams ToCameraParams(const CamParams &params)
{
    CameraParams out;
    out.exp_time = params.exp_time;
    out.exp_gain = params.exp_gain;
    out.lights = params.lights;
    out.focus_pos = params.focus_pos;
    return out;
}

CamParams ToCamParams(const CameraParams &params)
{
    CamParams out;
    out.exp_time = params.exp_time;
    out.exp_gain = params.exp_gain;
    out.lights = params.lights;
    out.focus_pos = params.focus_pos;
    return out;
}

CamParams InitCamParams(const CamConf &cam_conf)
{
    CamParams params;
    params.exp_time = cam_conf.init_et;
    params.exp_gain = cam_conf.init_eg;
    params.lights = cam_conf.init_intensities.empty() ? std::vector<int>{1, 1, 1, 1}
                                                      : cam_conf.init_intensities;
    // 界面/产品传入的 init_pos；钳到行程内，避免非法初值导致首帧大行程空跑。
    params.focus_pos =
        std::clamp(cam_conf.init_pos, cam_conf.start_pos, cam_conf.end_pos);
    return params;
}

// 产品未用 en_hmap：默认找板端 YOLO 模型；可用环境变量 AT_HEATMAP_MODEL 覆盖。
std::string ResolveDefaultHeatmapModel()
{
    if (const char *env = std::getenv("AT_HEATMAP_MODEL")) {
        if (env[0] != '\0' && std::filesystem::exists(env)) {
            return std::string(env);
        }
    }

    static const char *kCandidates[] = {
        "/usr/scanner/yolov8n-gray-test/model/barcode-yolov8n-gray-final-uint8.tmfile",
        "/usr/scanner/algorithm/barcode-yolov8n-gray-final-uint8.tmfile",
        "/tmp/at_runner/model/barcode-yolov8n-gray-final-uint8.tmfile",
    };
    for (const char *path : kCandidates) {
        if (std::filesystem::exists(path)) {
            return std::string(path);
        }
    }
    return {};
}

SessionConfig MapSessionConfig(const CamConf &cam_conf,
                               bool en_af,
                               bool en_ae,
                               bool en_ar)
{
    SessionConfig config;
    config.camera.min_exp_time = cam_conf.min_et;
    config.camera.max_exp_time = cam_conf.max_et;
    config.camera.min_exp_gain = cam_conf.min_eg;
    config.camera.max_exp_gain = cam_conf.max_eg;
    config.camera.min_light = cam_conf.min_intensity;
    config.camera.max_light = cam_conf.max_intensity;
    config.camera.min_focus_pos = cam_conf.start_pos;
    config.camera.max_focus_pos = cam_conf.end_pos;

    config.budget.exposure_step = std::max(1, cam_conf.min_et_step);
    config.budget.gain_step = 16;
    config.budget.focus_step =
        std::max(1, cam_conf.coarse_af_step > 0 ? cam_conf.coarse_af_step : 8);
    config.budget.max_steps = 80;
    config.budget.decode_budget = en_ar ? 6 : 0;

    config.flow.focus_tune_steps = en_af ? 8 : 1;
    config.flow.exposure_steps_per_profile = en_ae ? 6 : 1;
    config.flow.enable_heatmap = true;
    config.flow.enable_decode_ranking = en_ar;
    config.heatmap.model_path = ResolveDefaultHeatmapModel();
    config.heatmap.context = "timvx";
    config.heatmap.precision = "uint8";
    // YOLO 置信度门控（(0,1]）；与检测默认阈值对齐，避免 ranking 空转。
    config.heatmap.threshold = 0.25;
    config.run_log.enable = true;
    if (const char *log_dir = std::getenv("AT_LOG_DIR")) {
        if (log_dir[0] != '\0') {
            config.run_log.dir = log_dir;
        }
    }
    return config;
}

}  // namespace

struct ATInterface::Impl {
    bool en_hmap{false};  // 旧构造参数保留；Init 时忽略，默认走 YOLO
    bool initialized{false};
    bool finished{false};
    CamParams next_params{};
    CamParams best_params{};
    CamParams applied_params{};
    ARInfo ar_info{};
    cv::Rect manual_roi{};
    BarcodeWrapperBase *barcode{nullptr};
    compat::BarcodeDecodeProvider *decode_view{nullptr};  // owned by orchestrator
    std::unique_ptr<AtOrchestrator> orchestrator;

    void EnsureReady() const
    {
        if (!initialized || orchestrator == nullptr) {
            throw std::runtime_error("ATInterface is not initialized");
        }
    }

    void Setup(CamConf &cam_conf,
               BarcodeWrapperBase *barcode_wrapper,
               bool en_af,
               bool en_ae,
               bool en_ar)
    {
        SessionConfig config = MapSessionConfig(cam_conf, en_af, en_ae, en_ar);
        const std::string heatmap_model = config.heatmap.model_path;
        const std::string version = ATInterface::GetVersion();
        (void)en_hmap;

        AtRunLog::WriteInitStatus(config.run_log,
                                  version,
                                  heatmap_model,
                                  "loading",
                                  heatmap_model.empty()
                                      ? "model missing, will use NullHeatmap"
                                      : "YOLO provider async load (non-blocking Init)");

        barcode = barcode_wrapper;
        decode_view = nullptr;

        std::unique_ptr<DecodeProvider> decode;
        if (en_ar && barcode != nullptr) {
            auto bridge = std::make_unique<compat::BarcodeDecodeProvider>(barcode);
            decode_view = bridge.get();
            decode = std::move(bridge);
        }

        try {
            // 替换前先 Abort：给上一轮未 finished 的 run 写 end(Interrupted)。
            orchestrator.reset();
            orchestrator =
                std::make_unique<AtOrchestrator>(std::move(config), std::move(decode), nullptr);
            orchestrator->Reset();
        } catch (const std::exception &ex) {
            AtRunLog::WriteInitStatus(RunLogConfig{true, {}},
                                      version,
                                      heatmap_model,
                                      "error",
                                      ex.what());
            throw;
        }

        AtRunLog::WriteInitStatus(orchestrator->run_log().active()
                                      ? RunLogConfig{true, orchestrator->run_log().dir()}
                                      : RunLogConfig{true, {}},
                                  version,
                                  heatmap_model,
                                  "ready",
                                  heatmap_model.empty()
                                      ? std::string{}
                                      : "AT ready; YOLO session-scoped async load (no resident)",
                                  orchestrator->run_log().path());

        next_params = InitCamParams(cam_conf);
        best_params = next_params;
        applied_params = next_params;
        ar_info.reset();
        manual_roi = {};
        finished = false;
        initialized = true;

        if (barcode != nullptr) {
            barcode->Reset();
        }
    }
};

ATInterface::ATInterface() : ATInterface(false) {}

ATInterface::ATInterface(bool en_hmap) : impl_(std::make_unique<Impl>())
{
    impl_->en_hmap = en_hmap;
}

ATInterface::~ATInterface() = default;

ATInterface::ATInterface(ATInterface &&) noexcept = default;
ATInterface &ATInterface::operator=(ATInterface &&) noexcept = default;

void ATInterface::Init(CamConf &cam_conf, bool en_al, bool en_af, bool en_ae)
{
    (void)en_al;
    impl_->Setup(cam_conf, nullptr, en_af, en_ae, false);
}

void ATInterface::Init(CamConf &cam_conf,
                       BarcodeWrapperBase &barcode_wrapper,
                       bool en_al,
                       bool en_af,
                       bool en_ae,
                       bool en_ar)
{
    (void)en_al;
    impl_->Setup(cam_conf, &barcode_wrapper, en_af, en_ae, en_ar);
}

bool ATInterface::Run(const cv::Mat &image)
{
    impl_->EnsureReady();
    if (impl_->finished) {
        return true;
    }
    if (image.empty()) {
        throw std::runtime_error("ATInterface::Run requires a non-empty image");
    }

    // Product call order: GetNextParams -> set camera -> capture -> Run(image).
    impl_->applied_params = impl_->next_params;

    FrameContext context;
    context.image = image;
    context.current_params = ToCameraParams(impl_->applied_params);
    context.manual_roi = impl_->manual_roi;

    const StepResult result = impl_->orchestrator->ProcessStep(context);
    impl_->next_params = ToCamParams(result.next_params);

    if (result.best_candidate.has_value()) {
        impl_->best_params = ToCamParams(result.best_candidate->params);
    } else if (result.finished) {
        impl_->best_params = impl_->applied_params;
    }

    if (impl_->decode_view != nullptr) {
        impl_->ar_info = impl_->decode_view->LastArInfo();
    }

    impl_->finished = result.finished;
    if (impl_->finished) {
        // AT 结束即释放 YOLO，下次 Init（再点 AT）再加载。
        impl_->orchestrator->ReleaseHeatmap();
    }
    return impl_->finished;
}

void ATInterface::SetRoi(const cv::Rect &roi)
{
    if (impl_ == nullptr) {
        return;
    }
    impl_->manual_roi = roi;
}

CamParams ATInterface::GetNextParams()
{
    impl_->EnsureReady();
    return impl_->next_params;
}

CamParams ATInterface::GetBestParams()
{
    impl_->EnsureReady();
    return impl_->best_params;
}

ARInfo ATInterface::GetARInfo()
{
    impl_->EnsureReady();
    return impl_->ar_info;
}

std::string ATInterface::GetVersion()
{
    return std::string(GetLibVersion());
}

}  // namespace at
