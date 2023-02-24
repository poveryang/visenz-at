#include "at_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "hmap_generator.h"
#include "version.h"


namespace at {
    class ARInterface {
    public:
        ARInterface() = default;

        ~ARInterface() = default;

        void Init(BarcodeWrapperBase &barcode_wrapper) {
            barcode_wrapper_ = &barcode_wrapper;
            barcode_wrapper_->Reset();
        };

        void EndSet() {
            barcode_wrapper_->SetOriginParams();
        }

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

        bool end_iter = false;

    private:
        BarcodeWrapperBase *barcode_wrapper_{};
    };

    class ATInterface::ATImpl {
    public:
        al::ALInterface al_obj;
        ae::AEInterface ae4af_obj;
        af::AFInterface af_obj;
        ae::AEInterface ae_obj;
        ARInterface ar_obj;

        // TODO: refactor this
        std::string precision = "uint8";
        std::string model_path = "/tmp/hmap-v3-e99-uint8.tmfile";
        std::string context_name = "timvx";
        HeatMapGenerator hmap_generator = HeatMapGenerator(context_name, precision);

        ATImpl() {
            hmap_generator.Init(model_path);
        };

        ~ATImpl() = default;
    };

    ATInterface::ATInterface(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        at_impl_->ar_obj.Init(barcode_wrapper);
        cam_conf_ = cam_conf;
        SetInitParams(cam_conf);
    }

    void ATInterface::Init(bool enable_al, bool enable_af, bool enable_ae, bool enable_ar) {
        en_al_ = enable_al;
        en_af_ = enable_af;
        en_ae_ = enable_ae;
        en_ar_ = enable_ar;

        if (enable_al) {
            pipeline_.emplace_back(AL);
            at_impl_->al_obj.end_iter = false;
            at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
            // Just init the lights which are enabled
            for (int i = 0; i < best_params_.lights.size(); i++) {
                if (best_params_.lights[i] > 0) {
                    next_params_.lights[i] = at_impl_->al_obj.next_intensity;
                }
            }
        } else {
            at_impl_->al_obj.end_iter = true;
        }

        if (enable_af) {
            pipeline_.emplace_back(AE4AF);
            at_impl_->ae4af_obj.end_iter = false;
            at_impl_->ae4af_obj.Init(cam_conf_.AE_MODE,
                                     cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                     cam_conf_.MIN_EG, cam_conf_.MAX_EG);
            next_params_.exp_time = at_impl_->ae4af_obj.next_et;
            next_params_.exp_gain = at_impl_->ae4af_obj.next_eg;

            pipeline_.emplace_back(AF);
            at_impl_->af_obj.end_iter = false;
            at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS, cam_conf_.LENS_TYPE);
            next_params_.focus_pos = at_impl_->af_obj.next_pos;
        } else {
            at_impl_->af_obj.end_iter = true;
        }

        if (enable_ae) {
            pipeline_.emplace_back(AE);
            at_impl_->ae_obj.end_iter = false;
            at_impl_->ae_obj.Init(cam_conf_.AE_MODE,
                                  cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                  cam_conf_.MIN_EG, cam_conf_.MAX_EG);
            next_params_.exp_time = at_impl_->ae_obj.next_et;
            next_params_.exp_gain = at_impl_->ae_obj.next_eg;
        } else {
            at_impl_->ae_obj.end_iter = true;
        }

        if (enable_ar) {
            pipeline_.emplace_back(AR);
            at_impl_->ar_obj.end_iter = false;
        } else {
            at_impl_->ar_obj.end_iter = true;
        }

        pipeline_.emplace_back(END);
        cur_phase_ = pipeline_.begin();
    }

    ATInterface::ATInterface(CamConf &cam_conf) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        cam_conf_ = cam_conf;
        SetInitParams(cam_conf);
    }

    void ATInterface::Init(bool enable_al, bool enable_af, bool enable_ae) {
        en_al_ = enable_al;
        en_af_ = enable_af;
        en_ae_ = enable_ae;
        if (enable_al) {
            pipeline_.emplace_back(AL);
            at_impl_->al_obj.end_iter = false;
            at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
            for (int i = 0; i < best_params_.lights.size(); i++) {
                if (best_params_.lights[i] > 0) {
                    next_params_.lights[i] = at_impl_->al_obj.next_intensity;
                }
            }
        } else {
            at_impl_->al_obj.end_iter = true;
        }

        if (enable_af) {
            pipeline_.emplace_back(AE4AF);
            at_impl_->ae4af_obj.end_iter = false;
            at_impl_->ae4af_obj.Init(1,
                                     cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                     cam_conf_.MIN_EG, cam_conf_.MAX_EG);
            next_params_.exp_time = at_impl_->ae4af_obj.next_et;
            next_params_.exp_gain = at_impl_->ae4af_obj.next_eg;

            pipeline_.emplace_back(AF);
            at_impl_->af_obj.end_iter = false;
            at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS, cam_conf_.LENS_TYPE);
            next_params_.focus_pos = at_impl_->af_obj.next_pos;
        } else {
            at_impl_->af_obj.end_iter = true;
        }

        if (enable_ae) {
            pipeline_.emplace_back(AE);
            at_impl_->ae_obj.end_iter = false;
            at_impl_->ae_obj.Init(cam_conf_.AE_MODE,
                                  cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                  cam_conf_.MIN_EG, cam_conf_.MAX_EG);
            next_params_.exp_time = at_impl_->ae_obj.next_et;
            next_params_.exp_gain = at_impl_->ae_obj.next_eg;
        } else {
            at_impl_->ae_obj.end_iter = true;
        }

        pipeline_.emplace_back(END);
        cur_phase_ = pipeline_.begin();
    }

    bool ATInterface::Run(const cv::Mat &image) {
        SequentialExec(image);
        if (*cur_phase_ != END) {
            UpdateNextParams();
            return false;
        } else {
            if (!score_params_.empty()) {
                if (score_params_.rbegin()->first != 0) {
                    best_params_ = score_params_.rbegin()->second;
                } else if (en_ar_) {
                    at_impl_->ar_obj.EndSet();
                }
            }
            return true;
        }
    }

    void ATInterface::SequentialExec(const cv::Mat &image) {
        switch (*cur_phase_) {
            case AL:
                printf("[==>ViSenz-AL is in progress] ");
                at_impl_->al_obj.Run(image(image_roi_));
                break;
            case AE4AF:
                printf("[==>ViSenz-AE4AF is in progress] ");
                at_impl_->ae4af_obj.FastRun(image(image_roi_));
                break;
            case AF:
                printf("[==>ViSenz-AF is in progress] ");
                at_impl_->af_obj.Run(image(image_roi_));
                break;
            case AE:
                printf("[==>ViSenz-AE is in progress] ");
                if (at_impl_->ae_obj.first_run) {
                    if (en_ar_) {
                        score = at_impl_->ar_obj.GetScore(image, ar_params_);
                        printf("Barcode score = %.2f\n", score);
                    } else {
                        score = 0;
                    }
                    score_params_[score] = next_params_;
                    at_impl_->ae_obj.first_run = false;
                }
                at_impl_->ae_obj.Run(image(image_roi_));
                if (at_impl_->ae_obj.end_iter) {
                    if (en_ar_) {
                        score = at_impl_->ar_obj.GetScore(image, ar_params_);
                        printf("Barcode score = %.2f\n", score);
                    } else {
                        score = 0;
                    }
                    score_params_[score] = next_params_;
                }
                break;
            case AR:
                printf("[==>ViSenz-AR is in progress] ");
                score = at_impl_->ar_obj.GetScore(image, ar_params_);
                score_params_[score] = next_params_;
                printf("Barcode score = %.2f\n", score);
                std::cout << "[==>ViSenz-AR is done ]" << std::endl;
                break;
            case END: {
                printf("[==>ViSenz-HAMP is in progress] ");
                // preprocess image using hamp_generator
                cv::Mat image_copy = image.clone();
                cv::Mat in_image = at_impl_->hmap_generator.PreProcess(image_copy);
                cv::Mat hmap = at_impl_->hmap_generator.Infer(in_image);
                cv::Mat hmap_post = at_impl_->hmap_generator.PostProcess(hmap);
                cv::imwrite("/tmp/at_res/hmap.png", hmap_post);
                break;
            }
        }
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

    void ATInterface::SetInitParams(CamConf &cam_conf) {
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

    void ATInterface::UpdateNextParams() {
        if (*cur_phase_ == AL) {
            if (!at_impl_->al_obj.end_iter) {
                for (int i = 0; i < best_params_.lights.size(); i++) {
                    if (best_params_.lights[i] > 0) {
                        next_params_.lights[i] = at_impl_->al_obj.next_intensity;
                    }
                }
            } else {
                for (int &light: best_params_.lights) {
                    if (light > 0) {
                        light = at_impl_->al_obj.best_intensity;
                    }
                }
                next_params_.lights = best_params_.lights;
                cur_phase_++;
            }
        } else if (*cur_phase_ == AE4AF) {
            if (!at_impl_->ae4af_obj.end_iter) {
                next_params_.exp_time = at_impl_->ae4af_obj.next_et;
            } else {
                next_params_.exp_time = at_impl_->ae4af_obj.best_et;
                if (en_ae_) {
                    at_impl_->ae_obj.ResetParams(next_params_.exp_time);
                }
                cur_phase_++;
            }
        } else if (*cur_phase_ == AF) {
            if (!at_impl_->af_obj.end_iter) {
                next_params_.focus_pos = at_impl_->af_obj.next_pos;
            } else {
                best_params_.focus_pos = at_impl_->af_obj.best_pos;
                next_params_.focus_pos = best_params_.focus_pos;
                cur_phase_++;
            }
        } else if (*cur_phase_ == AE) {
            if (!at_impl_->ae_obj.end_iter) {
                next_params_.exp_time = at_impl_->ae_obj.next_et;
                next_params_.exp_gain = at_impl_->ae_obj.next_eg;
            } else {
                best_params_.exp_time = at_impl_->ae_obj.best_et;
                best_params_.exp_gain = at_impl_->ae_obj.best_eg;
                next_params_.exp_time = best_params_.exp_time;
                next_params_.exp_gain = best_params_.exp_gain;
                cur_phase_++;
            }
        } else if (*cur_phase_ == AR) {
            cur_phase_++;
        }
    }

    std::string ATInterface::GetVersion() {
        std::string at_version =
                std::to_string(AT_VERSION_MAJOR) + "." +
                std::to_string(AT_VERSION_MINOR) + "." +
                std::to_string(AT_VERSION_PATCH);

        return at_version;
    }
}