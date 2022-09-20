#include "AT_interface.h"
#include "al_interface.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "ar_interface.h"
#include <cstdio>

namespace at {
    class ATInterface::ATImpl {
    public:
        bool end_iter;
        bool at_fine_tune;
        bool al_fine_tune;
        bool af_fine_tune;
        bool ae_fine_tune;
        al::ALInterface al_obj;
        ae::AEInterface ae_obj;
        af::AFInterface af_obj;
        ar::ARInterface ar_obj;

        cv::Rect2i full_roi = {0, 0, 1280, 800};
        cv::Rect2i at_patch = full_roi;
        std::vector<cv::Rect2i> at_patches{};
        std::map<double, CamParams, std::greater<>> at_param_scores;

        ATImpl() {
            end_iter = false;
            at_fine_tune = false;
            al_fine_tune = false;
            af_fine_tune = false;
            ae_fine_tune = false;
        }

        ~ATImpl() = default;

        void Run(const cv::Mat &image, ATPhase &phase, ATConfig &config) {
            switch (phase) {
                case AL:
                    std::cout << "[==>ViSenz-AL is in progress ] ";
                    if (al_fine_tune){
                        al_obj.FineTune(image(at_patch));
                    } else {
                        al_obj.Run(image(at_patch));
                    }
                    break;
                case AF:
                    std::cout << "[==>ViSenz-AF is in progress ] ";
                    if (af_fine_tune){
                        af_obj.FineTune(image(at_patch));
                    } else {
                        af_obj.Run(image(at_patch));
                    }
                    break;
                case AE:
                    std::cout << "[==>ViSenz-AE is in progress ] ";
                    if (ae_fine_tune){
                        bool decode_success = false;
                        ar_obj.GetInfo(image, config.ar_config);
                        ar_obj.end_iter = false;
                        if (ar_obj.score > 0) {
                            decode_success = true;
                        }
                        ae_obj.FineTune(image(at_patch), decode_success);
                    } else {
                        ae_obj.Run(image(at_patch));
                    }
                    break;
                case AR:
                    std::cout << "[==>ViSenz-AR is in progress ] ";
                    ar_obj.GetInfo(image, config.ar_config);
                    break;
                case END:
                    break;
            }
        }

        void ResetAT(bool enable_al, bool enable_af, bool enable_ae, bool enable_ar, CamConf cam_conf, ATConfig &at_config) {
            if (enable_al) {
                al_obj.end_iter = false;
                if (al_fine_tune){
                    al_obj.Init(al_obj.best_intensity);
                }else{
                    al_obj.Init(cam_conf.MIN_INTENSITY, cam_conf.MAX_INTENSITY);
                }
                at_config.hardware.lights = {al_obj.next_intensity, al_obj.next_intensity,
                                            al_obj.next_intensity, al_obj.next_intensity};
            } else {
                al_obj.end_iter = true;
            }

            if (enable_af) {
                af_obj.end_iter = false;
                if (af_fine_tune){
                    af_obj.Init(af_obj.best_pos, cam_conf.START_POS, cam_conf.END_POS, 3);
                } else {
                    af_obj.Init(cam_conf.START_POS, cam_conf.END_POS, cam_conf.STEP_SIZE);
                }
                at_config.hardware.motor = af_obj.next_pos;

            } else {
                af_obj.end_iter = true;
            }

            if (enable_ae) {
                ae_obj.end_iter = false;
                if (ae_fine_tune){
                    ae_obj.Init(ae_obj.best_et, ae_obj.best_eg,
                                cam_conf.MIN_ET, cam_conf.MAX_ET, cam_conf.MIN_EG, cam_conf.MAX_EG);
                } else {
                    ae_obj.Init(cam_conf.MODE,
                                cam_conf.MIN_ET, cam_conf.MAX_ET, cam_conf.MIN_EG, cam_conf.MAX_EG);
                }
                at_config.hardware.exposure = ae_obj.next_et;
                at_config.hardware.gain = ae_obj.next_eg;
            } else {
                ae_obj.end_iter = true;
            }

            if (enable_ar) {
                ar_obj.score = 0;
                ar_obj.end_iter = false;
            } else {
                ar_obj.end_iter = true;
            }
        }
    };

    ATInterface::ATInterface() {
        end_iter = false;
        cur_phase = END;
        en_al = en_af = en_ae = en_ar = false;
        at_impl_ = std::make_shared<ATInterface::ATImpl>();
    }

    void ATInterface::run(const cv::Mat &image) {
        UpdateCurPhase();
        if (cur_phase != END) {
            at_impl_->Run(image, cur_phase, m_config);
            UpdateNextParams();
        } else {
            double best_score = at_impl_->at_param_scores.begin()->first;
            if (!at_impl_->at_patches.empty() && best_score <=0) {
                at_impl_->at_patch = at_impl_->at_patches.front();
                at_impl_->at_patches.erase(at_impl_->at_patches.begin());

                at_impl_->af_fine_tune = true;
                at_impl_->ResetAT(en_al, en_af, en_ae, en_ar, cam_conf_, m_config);
            } else if (at_impl_->at_fine_tune && m_config.ar_config.rect!=at_impl_->full_roi){
                en_al = false;
                en_ar = true;
                at_impl_->al_fine_tune = false;
                at_impl_->af_fine_tune = true;
                at_impl_->ae_fine_tune = true;
                at_impl_->at_patch = m_config.ar_config.rect;
                std::cout << "====>finetune roi " << at_impl_->at_patch<< std::endl;
                at_impl_->ResetAT(en_al, en_af, en_ae, en_ar, cam_conf_, m_config);
                at_impl_->at_fine_tune = false;
            } else{
                CamParams best_res = at_impl_->at_param_scores.begin()->second;//mark
                m_config.best_lights = best_res.lights;
                m_config.best_exposure = best_res.exposure_time;
                m_config.best_gain = best_res.exposure_gain;
                m_config.best_motor = best_res.focus_pos;
                m_config._end_iteration = true;
            }
        }
    }

    void ATInterface::init(bool enable_ae, bool enable_af, bool enable_al, bool enable_ar, int time) {
        en_al = enable_al;
        en_af = enable_af;
        en_ae = enable_ae;
        en_ar = enable_ar;

        if (time == 0) {
            at_impl_->at_patches = {};
        } else {
            en_ar = true;
            at_impl_->at_fine_tune = true;
            at_impl_->at_patches = {
                cv::Rect(0, 0, 640, 400),
                cv::Rect(640, 0, 640, 400),
                cv::Rect(0, 400, 640, 400),
                cv::Rect(640, 400, 640, 400),
                cv::Rect(320, 200, 640, 400),
            };

        }
        at_impl_->ResetAT(en_al, en_af, en_ae, en_ar, cam_conf_, m_config);
    }
    
    void ATInterface::init_hardware(at::ATConfig &hw_config) {
        printf("[==>ViSenz-Init is in progress ] \n"
               "Input Lights = %d, %d, %d, %d\n"
               "Input Exp Time = %d\n"
               "Input Exp Gain = %d\n"
               "Input Focus Pos = %d\n",
               hw_config.hardware.lights[0], hw_config.hardware.lights[1],
               hw_config.hardware.lights[2], hw_config.hardware.lights[3],
               hw_config.hardware.exposure, hw_config.hardware.gain, hw_config.hardware.motor);

        m_config.best_lights[0] = m_config.hardware.lights[0] = hw_config.hardware.lights[0];
        m_config.best_lights[1] = m_config.hardware.lights[1] = hw_config.hardware.lights[1];
        m_config.best_lights[2] = m_config.hardware.lights[2] = hw_config.hardware.lights[2];
        m_config.best_lights[3] = m_config.hardware.lights[3] = hw_config.hardware.lights[3];
        m_config.best_motor = m_config.hardware.motor = hw_config.hardware.motor;
        m_config.best_exposure = m_config.hardware.exposure = hw_config.hardware.exposure;
        m_config.best_gain = m_config.hardware.gain = hw_config.hardware.gain;

        m_config.hardware.roi = hw_config.hardware.roi;
        if (at_impl_->at_patch != hw_config.hardware.roi){
            at_impl_->at_patch = hw_config.hardware.roi;
            at_impl_->at_patches.clear();
        }

        if (en_al) {
            m_config.hardware.lights = {at_impl_->al_obj.next_intensity, at_impl_->al_obj.next_intensity,
                                        at_impl_->al_obj.next_intensity, at_impl_->al_obj.next_intensity};
        }

        if (en_ae) {
            m_config.hardware.exposure = at_impl_->ae_obj.next_et;
            m_config.hardware.gain = at_impl_->ae_obj.next_eg;
        }

        if (en_af) {
            at_impl_->af_obj.cur_et = m_config.hardware.exposure;
            m_config.hardware.motor = at_impl_->af_obj.next_pos;
        }

    }

    void ATInterface::init_ae(int mode, int min_et, int max_et, int min_eg, int max_eg) {
        at_impl_->ae_obj.Init(mode, min_et, max_et, min_eg, max_eg);
    }

    void ATInterface::setDevice(std::string &dev_name) {
        dev_name_ = dev_name;
        if (dev_name == "VS1000PRO") {
            cam_conf_ = {
                    .MIN_INTENSITY = 1,
                    .MAX_INTENSITY = 24,
                    .MODE = 0,
                    .MIN_ET = 0,
                    .MAX_ET = 10000,
                    .MIN_EG = 1,
                    .MAX_EG = 255,
                    .START_POS = 0,
                    .END_POS = 410,
                    .STEP_SIZE = 30
            };
        }
        else if (dev_name == "VS2000"){
            cam_conf_ = {
                    .MIN_INTENSITY = 1,
                    .MAX_INTENSITY = 24,
                    .MODE = 0,
                    .MIN_ET = 0,
                    .MAX_ET = 10000,
                    .MIN_EG = 1,
                    .MAX_EG = 255,
                    .START_POS = 0,
                    .END_POS = 410,
                    .STEP_SIZE = 30
            };
        }
        else if (dev_name == "VS800" | dev_name == "VN800") {
            cam_conf_ = {
                    .MIN_INTENSITY = 24,
                    .MAX_INTENSITY = 24,
                    .MODE = 0,
                    .MIN_ET = 0,
                    .MAX_ET = 10000,
                    .MIN_EG = 1,
                    .MAX_EG = 255,
                    .START_POS = 0,
                    .END_POS = 1023,
                    .STEP_SIZE = 30
            };
        }
    }

    void ATInterface::UpdateCurPhase() {
        if (!at_impl_->al_obj.end_iter) {
            cur_phase = AL;
        } else if (!at_impl_->af_obj.end_iter) {
            cur_phase = AF;
        } else if (!at_impl_->ae_obj.end_iter) {
            cur_phase = AE;
        } else if (!at_impl_->ar_obj.end_iter) {
            cur_phase = AR;
        } else {
            cur_phase = END;
        }
    }

    void ATInterface::UpdateNextParams() {
        if (cur_phase == AL) {
            if (!at_impl_->al_obj.end_iter) {
                m_config.hardware.lights[0] = at_impl_->al_obj.next_intensity;
                m_config.hardware.lights[1] = at_impl_->al_obj.next_intensity;
                m_config.hardware.lights[2] = at_impl_->al_obj.next_intensity;
                m_config.hardware.lights[3] = at_impl_->al_obj.next_intensity;
            } else {
                m_config.best_lights[0] = at_impl_->al_obj.best_intensity;
                m_config.best_lights[1] = at_impl_->al_obj.best_intensity;
                m_config.best_lights[2] = at_impl_->al_obj.best_intensity;
                m_config.best_lights[3] = at_impl_->al_obj.best_intensity;
                m_config.hardware.lights[0] = m_config.best_lights[0];
                m_config.hardware.lights[1] = m_config.best_lights[1];
                m_config.hardware.lights[2] = m_config.best_lights[2];
                m_config.hardware.lights[3] = m_config.best_lights[3];
            }
        } else if (cur_phase == AF) {
            at_impl_->af_obj.next_et = std::max(cam_conf_.MIN_ET,at_impl_->af_obj.next_et);
            at_impl_->af_obj.next_et = std::min(cam_conf_.MAX_ET,at_impl_->af_obj.next_et);
            if (at_impl_->af_obj.next_et > 0) {
                m_config.hardware.exposure = at_impl_->af_obj.next_et;
                at_impl_->ae_obj.ResetParams(at_impl_->af_obj.next_et);
            }
            if (!at_impl_->af_obj.end_iter) {
                m_config.hardware.motor = at_impl_->af_obj.next_pos;
            } else {
                m_config.best_motor = at_impl_->af_obj.best_pos;
                m_config.hardware.motor = m_config.best_motor;
            }
        } else if (cur_phase == AE) {
            if (!at_impl_->ae_obj.end_iter) {
                m_config.hardware.exposure = at_impl_->ae_obj.next_et;
                m_config.hardware.gain = at_impl_->ae_obj.next_eg;
            } else {
                m_config.best_exposure = at_impl_->ae_obj.best_et;
                m_config.best_gain = at_impl_->ae_obj.best_eg;
                m_config.hardware.exposure = m_config.best_exposure;
                m_config.hardware.gain = m_config.best_gain;
            }
        } else if (cur_phase == AR) {
            double score = at_impl_->ar_obj.score;
            printf("Score = %.2f", at_impl_->ar_obj.score);
            CamParams at_param = {
                    m_config.best_lights,
                    m_config.best_motor,
                    m_config.best_exposure,
                    m_config.best_gain};

            at_impl_->at_param_scores.insert(std::make_pair(score,at_param));
        }
    }

    void ATInterface::setMetrics_Barcode(smartmore::barcode::Barcode &barcode_sdk) {
        at_impl_->ar_obj.Init(barcode_sdk);
    }
}