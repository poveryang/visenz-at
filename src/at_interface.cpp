#include "at_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "ar_interface.h"
#include "cam_config.h"

namespace at {
    class ATInterface::ATImpl {
    public:
        al::ALInterface al_obj;
        ae::AEInterface ae_obj;
        af::AFInterface af_obj;
        at::ARInterface ar_obj;

        ATImpl() = default;
        ~ATImpl() = default;
    };

    ATInterface::ATInterface(std::string &dev_name, CamParams &init_params, BarcodeWrapperBase &barcode_wrapper) {
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
        cur_phase_ = END;
        en_al_ = en_af_ = en_ae_ = en_ar_ = false;
        barcode_wrapper_ = &barcode_wrapper;
        SetDevice(dev_name, init_params);
    }

    void ATInterface::Init(bool enable_ae, bool enable_af, bool enable_al, bool enable_ar) {
        if (enable_al) {
            at_impl_->al_obj.end_iter = false;
            at_impl_->al_obj.Init(cam_conf_.MIN_INTENSITY, cam_conf_.MAX_INTENSITY);
        } else {
            at_impl_->al_obj.end_iter = true;
        }

        if (enable_af) {
            at_impl_->af_obj.end_iter = false;
            at_impl_->af_obj.Init(cam_conf_.START_POS, cam_conf_.END_POS);
        } else {
            at_impl_->af_obj.end_iter = true;
        }

        if (enable_ae) {
            at_impl_->ae_obj.end_iter = false;
            at_impl_->ae_obj.Init(cam_conf_.MIN_ET, cam_conf_.MAX_ET,
                                  cam_conf_.MIN_EG, cam_conf_.MAX_EG);
        } else {
            at_impl_->ae_obj.end_iter = true;
        }

        if (enable_ar) {
            at_impl_->ar_obj.end_iter = false;
            at_impl_->ar_obj.Init(*barcode_wrapper_);
        } else {
            at_impl_->ar_obj.end_iter = true;
        }
    }

    bool ATInterface::Run(const cv::Mat &image) {
        UpdateCurPhase();
        if (cur_phase_ != END) {
            SequentialExec(image);
            UpdateNextParams();
            return false;
        }
        return true;
    }

    void ATInterface::SequentialExec(const cv::Mat &image){
        switch (cur_phase_) {
            case AL:
                printf("[==>ViSenz-AL is in progress] ");
                at_impl_->al_obj.Run(image(image_roi_));
                break;
            case AF:
                printf("[==>ViSenz-AL is in progress] ");
                at_impl_->af_obj.Run(image(image_roi_));
                break;
            case AE:
                printf("[==>ViSenz-AL is in progress] ");
                at_impl_->ae_obj.Run(image(image_roi_));
                break;
            case AR:
                printf("[==>ViSenz-AL is in progress] ");
                at_impl_->ar_obj.GetInfo(image, ar_params_);
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

    void ATInterface::UpdateCurPhase() {
        if (!at_impl_->al_obj.end_iter) {
            cur_phase_ = AL;
        } else if (!at_impl_->af_obj.end_iter) {
            cur_phase_ = AF;
        } else if (!at_impl_->ae_obj.end_iter) {
            cur_phase_ = AE;
        } else if (!at_impl_->ar_obj.end_iter) {
            cur_phase_ = AR;
        } else {
            cur_phase_ = END;
        }
    }

    void ATInterface::UpdateNextParams() {
        if (cur_phase_ == AL) {
            if (!at_impl_->al_obj.end_iter) {
                next_params_.lights[0] = at_impl_->al_obj.next_intensity;
                next_params_.lights[1] = at_impl_->al_obj.next_intensity;
                next_params_.lights[2] = at_impl_->al_obj.next_intensity;
                next_params_.lights[3] = at_impl_->al_obj.next_intensity;
            } else {
                best_params_.lights[0] = at_impl_->al_obj.best_intensity;
                best_params_.lights[1] = at_impl_->al_obj.best_intensity;
                best_params_.lights[2] = at_impl_->al_obj.best_intensity;
                best_params_.lights[3] = at_impl_->al_obj.best_intensity;
                
                next_params_.lights = best_params_.lights;
            }
        } else if (cur_phase_ == AF) {
            if (!at_impl_->af_obj.end_iter) {
                next_params_.focus_pos = at_impl_->af_obj.next_pos;
            } else {
                best_params_.focus_pos = at_impl_->af_obj.best_pos;

                next_params_.focus_pos = best_params_.focus_pos;
            }
        } else if (cur_phase_ == AE) {
            if (!at_impl_->ae_obj.end_iter) {
                next_params_.exp_time = at_impl_->ae_obj.next_et;
                next_params_.exp_gain = at_impl_->ae_obj.next_eg;
            } else {
                best_params_.exp_time = at_impl_->ae_obj.best_et;
                best_params_.exp_gain = at_impl_->ae_obj.best_eg;

                next_params_.exp_time = best_params_.exp_time;
                next_params_.exp_gain = best_params_.exp_gain;
            }
        } else if (cur_phase_ == AR) {
            //todo: To be perfected
        }
    }

}