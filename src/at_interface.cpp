#include "at_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "cam_config.h"

namespace at {
    class ARInterface {
    public:
        ARInterface() = default;
        ~ARInterface() = default;

        void Init(BarcodeWrapperBase &barcode_wrapper){
            barcode_wrapper_ = &barcode_wrapper;
            barcode_wrapper_->Reset();
        };

        void GetInfo(const cv::Mat &image, at::ARParams &ar_params){
            std::vector<cv::Rect> rects = barcode_wrapper_->Decode(image, ar_params);
            if (!rects.empty()) {
                std::cout << "Recognized Barcode Info is as follows:" << std::endl;
                ar_params.print();
                ar_params.rect = rects[0];
            }
            end_iter = true;
        };

        double GetScore(const cv::Mat &image, at::ARParams &ar_params){
            double score = 0;
            std::vector<cv::Rect> rects = barcode_wrapper_->Decode(image, ar_params);
            for (const auto& rect: rects){
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

        ATImpl() = default;

        ~ATImpl() = default;
    };

    ATInterface::ATInterface(std::string &dev_name, CamParams &init_params, BarcodeWrapperBase &barcode_wrapper) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        barcode_wrapper_ = &barcode_wrapper;
        at_impl_->ar_obj.Init(*barcode_wrapper_);
        SetDevice(dev_name, init_params);
    }

    void ATInterface::Init(bool enable_al, bool enable_af, bool enable_ae, bool enable_ar) {
        if (enable_al) {
            pipeline_.emplace_back(AL);
            at_impl_->al_obj.end_iter = false;
            at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
            for (auto &light: next_params_.lights) {
                light = at_impl_->al_obj.next_intensity;
            }
        } else {
            at_impl_->al_obj.end_iter = true;
        }

        if (enable_af) {
            pipeline_.emplace_back(AE4AF);
            at_impl_->ae4af_obj.end_iter = false;
            at_impl_->ae4af_obj.Init(cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                     cam_conf_.MIN_EG, cam_conf_.MAX_EG);
            next_params_.exp_time = at_impl_->ae4af_obj.next_et;
            next_params_.exp_gain = at_impl_->ae4af_obj.next_eg;

            pipeline_.emplace_back(AF);
            at_impl_->af_obj.end_iter = false;
            at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS);
            next_params_.focus_pos = at_impl_->af_obj.next_pos;
        } else {
            at_impl_->af_obj.end_iter = true;
        }

        if (enable_ae) {
            pipeline_.emplace_back(AE);
            at_impl_->ae_obj.end_iter = false;
            at_impl_->ae_obj.Init(cam_conf_.MIN_ET, cam_conf_.MAX_ET,
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
        if (*cur_phase_ != END) {
            SequentialExec(image);
            UpdateNextParams();
            return false;
        } else {
            best_params_ = score_params_.rbegin()->second;
            return true;
        }
    }

    void ATInterface::SequentialExec(const cv::Mat &image){
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
                if (at_impl_->ae_obj.first_run){
                    double score;
                    score = at_impl_->ar_obj.GetScore(image, ar_params_);
                    score_params_[score] = next_params_;
                    printf("Barcode score = %.2f\n", score);
                    at_impl_->ae_obj.first_run = false;
                }
                at_impl_->ae_obj.Run(image(image_roi_));
                break;
            case AR:
                printf("[==>ViSenz-AR is in progress] ");
                double score;
                score = at_impl_->ar_obj.GetScore(image, ar_params_);
                score_params_[score] = next_params_;
                printf("Barcode score = %.2f\n", score);
                std::cout << "[==>ViSenz-AR is done ]" << std::endl;
                break;
            case END:
                break;
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

    void ATInterface::SetDevice(std::string &dev_name, CamParams &init_params) {
        // Sets the camera device configuration
        if (dev_name == "VS1000P") {
            cam_conf_ = vs1000p_conf;
        } else if (dev_name == "VS1000P@2M"){
            cam_conf_ = vs1000p2m_conf;
        } else if (dev_name == "VS800") {
            cam_conf_ = vs800_conf;
        } else if (dev_name == "VS2000"){
            cam_conf_ = vs2000_conf;
        }

        // Loads init camera parameters
        next_params_ = init_params;
        best_params_ = init_params;
        image_roi_ = init_params.roi;
    }

    void ATInterface::UpdateNextParams() {
        if (*cur_phase_ == AL) {
            if (!at_impl_->al_obj.end_iter) {
                for (auto &light: next_params_.lights) {
                    light = at_impl_->al_obj.next_intensity;
                }
            } else {
                for (auto &light: best_params_.lights) {
                    light = at_impl_->al_obj.best_intensity;
                }
                next_params_.lights = best_params_.lights;
                cur_phase_++;
            }
        } else if (*cur_phase_ == AE4AF) {
            if (!at_impl_->ae4af_obj.end_iter) {
                next_params_.exp_time = at_impl_->ae4af_obj.next_et;
            } else {
                next_params_.exp_time = at_impl_->ae4af_obj.best_et;
                at_impl_->ae_obj.ResetParams(next_params_.exp_time);
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
                cur_phase_++;
            } else {
                best_params_.exp_time = at_impl_->ae_obj.best_et;
                best_params_.exp_gain = at_impl_->ae_obj.best_eg;
                next_params_.exp_time = best_params_.exp_time;
                next_params_.exp_gain = best_params_.exp_gain;
            }
        } else if (*cur_phase_ == AR){
            cur_phase_ ++;
        }
    }

    std::string ATInterface::GetVersion() {
        // Engineering Version Number
        #define TRIA_VERSION_E_MAJOR 3
        #define TRIA_VERSION_E_MINOR 3
        #define TRIA_VERSION_E_PATCH 2
        #define TRIA_VERSION_E_RC    1

        #define AUX_STR_EXP(__A) #__A
        #define AUX_STR(__A) AUX_STR_EXP(__A)
        #define TRIA_VERSION_E                             \
            "v" AUX_STR(TRIA_VERSION_E_MAJOR) "." AUX_STR( \
                TRIA_VERSION_E_MINOR) "." AUX_STR(TRIA_VERSION_E_PATCH)
        #define TRIA_VERSION_RC  "-rc" AUX_STR(TRIA_VERSION_E_RC)

        std::string version = std::string(TRIA_VERSION_E);
        if (TRIA_VERSION_E_RC != 0) {
            version += TRIA_VERSION_RC;
        }
        return version;
    }
}