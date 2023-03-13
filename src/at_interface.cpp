#include "at_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "hmap_generator.h"
#include "exp_fusion.h"
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

    class ExpFusionInterface {
    public:
        ExpFusionInterface() {
            hmap_generator.Init(model_path_);
            end_iter = false;
            obj_detected = false;
            initialized = false;
            next_et = -1;
        }

        void Init(int cur_exp_time) {
            int prev_exp_time = cur_exp_time / 2;
            int next_exp_time = cur_exp_time + cur_exp_time / 2;
            exp_times_.push_back(cur_exp_time);
            exp_times_.push_back(next_exp_time);
            exp_times_.push_back(prev_exp_time);
            next_et = cur_exp_time;
        }

        void Run(const cv::Mat &image) {
            cv::Mat image_clone;
            image.convertTo(image_clone, CV_32FC1, 1.0 / 255);
            images_.push_back(image_clone);
            std::string image_name = "/tmp/at_res/fusion_" + std::to_string(next_et) + ".png";
            cv::imwrite(image_name, image);
            printf("push image to fusion array, exp time: %d\n", next_et);
            if (images_.size() == 3) {
                int64 start = cv::getTickCount();
                cv::Mat fusion_image = exp_fusion_.Process(images_);
                int64 end = cv::getTickCount();
                printf("fusion time: %f ms\n", double(end - start) / cv::getTickFrequency() * 1000);
                fusion_image.convertTo(fusion_image, CV_8UC1, 255.0);
                cv::imwrite("/tmp/at_res/fusion_image.png", fusion_image);
                cv::Mat hmap = hmap_generator.Infer(fusion_image);
                cv::imwrite("/tmp/at_res/hmap_fusion.png", hmap);
                image_roi = hmap_generator.FindRect(hmap);
                if (image_roi.area() > 100) {
                    obj_detected = true;
                    // expand roi
                    int roi_width = image_roi.width;
                    int roi_height = image_roi.height;
                    int roi_x = image_roi.x;
                    int roi_y = image_roi.y;
                    int expand_width = roi_width / 4;
                    int expand_height = roi_height / 4;
                    int expand_x = roi_x - expand_width;
                    int expand_y = roi_y - expand_height;
                    if (expand_x < 0) {
                        expand_x = 0;
                    }
                    if (expand_y < 0) {
                        expand_y = 0;
                    }
                    image_roi.x = expand_x;
                    image_roi.y = expand_y;
                    image_roi.width = roi_width + expand_width * 2;
                    image_roi.height = roi_height + expand_height * 2;
                    cv::rectangle(hmap, image_roi, cv::Scalar(0, 0, 255), 2);
                    cv::imwrite("/tmp/at_res/hmap_fusion.png", hmap);

                }
                end_iter = true;
            }
            next_et = exp_times_.back();
            exp_times_.pop_back();
        }

        bool end_iter;
        bool obj_detected;
        bool initialized;
        int next_et;
        cv::Rect image_roi;

    private:
//        std::string precision_ = "uint8";
//        std::string model_path_ = "/tmp/hmap-uint8-v7k-kl.tmfile";
//        std::string context_name_ = "timvx";

        std::string precision_ = "fp32";
        std::string model_path_ = "/tmp/hmap-v3-e99-fp32.tmfile";
        std::string context_name_ = "cpu";
        HeatMapGenerator hmap_generator = HeatMapGenerator(context_name_, precision_);

        std::vector<cv::Mat> images_;
        std::vector<int> exp_times_;

        ExpFusion exp_fusion_ = ExpFusion();
    };

    class ATInterface::ATImpl {
    public:
        al::ALInterface al_obj;
        ae::AEInterface ae4af_obj;
        af::AFInterface af_obj;
        ExpFusionInterface ef_obj;
        ae::AEInterface ae_obj;
        ARInterface ar_obj;

        ATImpl() = default;
        ~ATImpl() = default;
    };

    ATInterface::ATInterface(CamConf &cam_conf) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        cam_conf_ = cam_conf;
        SetInitParams(cam_conf);
}

    ATInterface::ATInterface(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        at_impl_->ar_obj.Init(barcode_wrapper);
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
            pipeline_.emplace_back(EF);
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
            case EF:
                printf("[==>ViSenz-EF is in progress] ");
                if (!at_impl_->ef_obj.initialized) {
                    at_impl_->ef_obj.Init(next_params_.exp_time);
                    at_impl_->ef_obj.initialized = true;
                }
                at_impl_->ef_obj.Run(image(image_roi_));
                break;
            case AE:
                printf("[==>ViSenz-AE is in progress] ");
                at_impl_->ae_obj.Run(image(image_roi_));
                if (object_detected_){
                    score = CalcScore(image(image_roi_));
                    score_params_[score] = next_params_;
                    printf("score = %.2f\n", score);
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
        } else if (*cur_phase_ == EF) {
            next_params_.exp_time = at_impl_->ef_obj.next_et;
            if (at_impl_->ef_obj.end_iter) {
                object_detected_ = at_impl_->ef_obj.obj_detected;
                if (object_detected_) {
                    image_roi_ = at_impl_->ef_obj.image_roi;
                }
                printf("Object detected: %d\n", object_detected_);
                cur_phase_++;
            }
        }
        else if (*cur_phase_ == AE) {
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

    double ATInterface::CalcScore(const cv::Mat &image) {
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

    std::string ATInterface::GetVersion() {
        std::string at_version =
                std::to_string(AT_VERSION_MAJOR) + "." +
                std::to_string(AT_VERSION_MINOR) + "." +
                std::to_string(AT_VERSION_PATCH);

        return at_version;
    }
}