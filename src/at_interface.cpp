#include "at_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "hmap_generator.h"
#include "version.h"


using namespace at;

class ARInterface {
public:
    ARInterface() = default;

    ~ARInterface() = default;

    void Init(BarcodeWrapperBase &barcode_wrapper) {
        barcode_wrapper_ = &barcode_wrapper;
        barcode_wrapper_->Reset();
    };

    void Decode(const cv::Mat &image, at::ARParams &ar_params) {
        barcode_wrapper_->Decode(image, ar_params);
    };

    void ResetOriParams() {
        barcode_wrapper_->SetOriginParams();
    }

    // TODO: deprecated
    double GetScore(const cv::Mat &image, at::ARParams &ar_params) {
        double score = 0;
        at::ARParams ar_params_tmp;
        std::vector<cv::Rect> rects = barcode_wrapper_->Decode(image, ar_params_tmp);
        if (!rects.empty()) {
            ar_params = ar_params_tmp;
        }

        for (const auto &rect: rects) {
            double tmp = CalcScore(image(rect));
            score += tmp;
        }
        return score;
    };

    // TODO: deprecated
    static double CalcScore(const cv::Mat &image) {
        cv::Mat dst;
        double thresh = cv::threshold(image, dst, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        float low_histogram[256] = {0};
        float high_histogram[256] = {0};
        float mean_low_brightness = 0, mean_high_brightness = 0;
        float low_nums = 0, high_nums = 0;
        for (int i = 0; i < image.rows; ++i) {
            for (int j = 0; j < image.cols; ++j) {
                auto pixel = image.ptr<uchar>(i)[j];
                if (pixel < thresh) {
                    low_histogram[pixel]++;
                } else {
                    high_histogram[pixel]++;
                }
            }
        }
        for (int i = 0; i < 256; ++i) {
            mean_low_brightness += (static_cast<float>(i) * low_histogram[i]);
            mean_high_brightness += (static_cast<float>(i) * high_histogram[i]);
            low_nums += low_histogram[i];
            high_nums += high_histogram[i];
        }
        mean_low_brightness /= low_nums;
        mean_high_brightness /= high_nums;
        return mean_high_brightness - mean_low_brightness;
    };

public:
    bool end_iter = false;

private:
    BarcodeWrapperBase *barcode_wrapper_{};
};

class ATInterface::ATImpl {
public:

    ATImpl() {
        enable_hmap = true;
        hmap_generator.Init("/tmp/hmap-v2-qat-uint8.tmfile");
    }

    ~ATImpl() = default;

public:
    /* sub modules */
    al::ALInterface al_obj;
    af::AFInterface af_obj;
    ae::AEInterface ae_obj;
    ARInterface ar_obj;

    /* heat-map generator */
    bool enable_hmap;
    HeatMapGenerator hmap_generator = HeatMapGenerator("timvx", "uint8");
};

ATInterface::ATInterface(CamConf &cam_conf) {
    at_impl_ = std::make_shared<ATInterface::ATImpl>();
    cam_conf_ = cam_conf;
    InitParams(cam_conf);
    en_ae_ = false;
    en_hmap_ = false;
}

ATInterface::ATInterface(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper) {
    at_impl_ = std::make_shared<ATInterface::ATImpl>();
    cam_conf_ = cam_conf;
    InitParams(cam_conf);
    en_ae_ = false;
    en_hmap_ = false;
    at_impl_->ar_obj.Init(barcode_wrapper);
}

void ATInterface::Init(bool enable_al, bool enable_af, bool enable_ae) {
    /* By default, AE is executed first to init the exp params */
    en_ae_ = enable_ae;
    pipeline_.emplace_back(AEQT);
    next_params_.exp_time = at_impl_->ae_obj.next_et;
    next_params_.exp_gain = at_impl_->ae_obj.next_eg_quant;

    /* Init AL and set the next intensities if AL is enabled */
    if (enable_al) {
        pipeline_.emplace_back(AL);
        at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
        for (int i = 0; i < best_params_.lights.size(); i++) {
            next_params_.lights[i] = at_impl_->al_obj.next_intensity;
        }
    } else {
        at_impl_->al_obj.end_tuning = true;
    }

    /* Init AF and set the next focus position if AF is enabled */
    if (enable_af) {
        pipeline_.emplace_back(AF);
        at_impl_->af_obj.end_iter = false;
        at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS, false);
        next_params_.focus_pos = at_impl_->af_obj.next_pos;
    } else {
        at_impl_->af_obj.end_iter = true;
    }

    /* Append AEST to the pipeline if AE is enabled */
    if (enable_ae) {
        pipeline_.emplace_back(AEST);
    }

    /* Append END to the pipeline by default */
    pipeline_.emplace_back(END);
    cur_phase_ = pipeline_.begin();
}

void ATInterface::Init(bool enable_al, bool enable_af, bool enable_ae, bool enable_ar, bool enable_hmap) {
    /* By default, AE is executed first to init the exp params */
    en_ae_ = enable_ae;
    pipeline_.emplace_back(AEQT);
    at_impl_->ae_obj.Init(cam_conf_.AE_MODE, cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                          cam_conf_.MIN_EG, cam_conf_.MAX_EG, cam_conf_.EG_QUANT_SCALE, enable_hmap);
    next_params_.exp_time = at_impl_->ae_obj.next_et;
    next_params_.exp_gain = at_impl_->ae_obj.next_eg_quant;

    /* Init AL and set the next intensities if AL is enabled */
    if (enable_al) {
        pipeline_.emplace_back(AL);
        at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
        for (auto &light : next_params_.lights) {
            light = at_impl_->al_obj.next_intensity;
        }
    } else {
        at_impl_->al_obj.end_tuning = true;
    }

    /* Init AF and set the next focus position if AF is enabled */
    if (enable_af) {
        pipeline_.emplace_back(AF);
        at_impl_->af_obj.end_iter = false;
        at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS, enable_hmap);
        next_params_.focus_pos = at_impl_->af_obj.next_pos;
    } else {
        at_impl_->af_obj.end_iter = true;
    }

    /* Append AEST to the pipeline if AE is enabled */
    if (enable_ae) {
        pipeline_.emplace_back(AEST);
    }

    /* Append AR to the pipeline if AR is enabled */
    if (enable_ar) {
        pipeline_.emplace_back(AR);
    }

    /* Append END to the pipeline by default */
    pipeline_.emplace_back(END);
    cur_phase_ = pipeline_.begin();
}

bool ATInterface::Run(const cv::Mat &image) {
    /* Execute the different phases sequentially */
    SequentialExec(image);

    /* Return true if the pipeline is finished or update the next params otherwise */
    if (*cur_phase_ == END) {
        return true;
    } else {
        UpdateNextParams();
        return false;
    }
}

void ATInterface::SequentialExec(const cv::Mat &image) {
    switch (*cur_phase_) {
        case AEQT: {
            printf("====>>> Executing AEQT\n");
            at_impl_->ae_obj.QuickTune(image);
            break;
        }
        case AEST: {
            printf("====>>> Executing AEST\n");
            if (at_impl_->ae_obj.enable_hmap){
                cv::Mat hmap = at_impl_->hmap_generator.Infer(image);
                Hotspot max_hot = at_impl_->hmap_generator.LocateMaxHotspot(hmap);
                printf("====>>> Executing AEST with hmap, intensity = %.2f\n", max_hot.mean_intensity);
                at_impl_->ae_obj.StepTune(image, max_hot.rect, max_hot.mean_intensity);
            } else {
                at_impl_->ae_obj.StepTune(image);
            }
            break;
        }
        case AL: {
            printf("====>>> Executing AL\n");
            at_impl_->al_obj.Tune(image);
            break;
        }
        case AF: {
            printf("====>>> Executing AF\n");
            if (at_impl_->enable_hmap) {
                cv::Mat hmap = at_impl_->hmap_generator.Infer(image);
                std::vector<Hotspot> hots = at_impl_->hmap_generator.LocateHotspots(hmap);
                printf("====>>> Executing AF with hmap, hots = %zu\n", hots.size());
                at_impl_->af_obj.Run(image, hots);
            } else {
                at_impl_->af_obj.Run(image);
            }
            break;
        }
        case AR: {
            printf("====>>> Executing AR\n");
            at_impl_->ar_obj.Decode(image, ar_params_);
            break;
        }
        case END: {
            printf("====>>> Executing END\n");
            break;
        }
    }
}

void ATInterface::UpdateNextParams() {
    switch (*cur_phase_) {
        case AL: {
            if (at_impl_->al_obj.end_tuning) {
                for (auto &light: best_params_.lights) {
                    light = at_impl_->al_obj.best_intensity;
                }
                next_params_.lights = best_params_.lights;
                at_impl_->enable_hmap = true;
                cur_phase_++; // Move to the next phase
            } else {
                for (auto &light: next_params_.lights) {
                    light = at_impl_->al_obj.next_intensity;
                }
            }
        }
        case AEQT: {
            if (at_impl_->ae_obj.end_tuning) {
                if (en_ae_) {
                    at_impl_->ae_obj.SwitchMode(-1);
                    at_impl_->ae_obj.end_tuning = false;
                }
                next_params_.exp_time = at_impl_->ae_obj.best_et;
                next_params_.exp_gain = at_impl_->ae_obj.best_eg_quant;
                cur_phase_++; // Move to the next phase
            } else {
                next_params_.exp_time = at_impl_->ae_obj.next_et;
                next_params_.exp_gain = at_impl_->ae_obj.next_eg_quant;
            }
            break;
        }
        case AEST: {
            if (at_impl_->ae_obj.end_tuning) {
                next_params_.exp_time = best_params_.exp_time = at_impl_->ae_obj.best_et;
                next_params_.exp_gain = best_params_.exp_gain = at_impl_->ae_obj.best_eg_quant;
                cur_phase_++; // Move to the next phase
            } else {
                next_params_.exp_time = at_impl_->ae_obj.next_et;
                next_params_.exp_gain = at_impl_->ae_obj.next_eg_quant;
            }
            break;
        }
        case AF: {
            if (at_impl_->af_obj.end_iter) {
                next_params_.focus_pos = best_params_.focus_pos = at_impl_->af_obj.best_pos;
                cur_phase_++; // Move to the next phase
            } else {
                next_params_.focus_pos = at_impl_->af_obj.next_pos;
            }
            break;
        }
        case AR: {
            if (!ar_params_.read_1D && !ar_params_.read_2D) {
                at_impl_->ar_obj.ResetOriParams();
            }
            cur_phase_++; // Move to the next phase
            break;
        }
        case END:
            break;
    }
}

void ATInterface::InitParams(CamConf &cam_conf) {
    next_params_.lights = cam_conf.INIT_INTENSITIES;
    next_params_.exp_time = cam_conf.INIT_ET;
    next_params_.exp_gain = cam_conf.INIT_EG;
    next_params_.focus_pos = cam_conf.INIT_POS;

    best_params_.lights = cam_conf.INIT_INTENSITIES;
    best_params_.exp_time = cam_conf.INIT_ET;
    best_params_.exp_gain = cam_conf.INIT_EG;
    best_params_.focus_pos = cam_conf.INIT_POS;

    image_roi_ = cam_conf.ROI;
}

std::string ATInterface::GetVersion() {
    std::string at_version =
            std::to_string(AT_VERSION_MAJOR) + "." +
            std::to_string(AT_VERSION_MINOR) + "." +
            std::to_string(AT_VERSION_PATCH);

    return at_version;
}

CamParams ATInterface::GetNextParams() {
    return next_params_;
}

CamParams ATInterface::GetBestParams() {
    return best_params_;
}

ARParams ATInterface::GetARParams() {
    return ar_params_;
}


