#include "at_vs_impl.h"

AT4VsImpl::AT4VsImpl(bool enable_hmap) {
    /* Load heat-map generator if enabled */
    enable_hmap_ = enable_hmap;
    if (enable_hmap_) {
        hmap_obj.Init("/usr/scanner/algorithm/hmap-v2-qat-uint8.tmfile");
    }
}

void AT4VsImpl::Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
                     bool en_al, bool en_af, bool en_ae, bool en_ar) {
    printf("[AT4VS] Initializing: \nAL(%d), AF(%d), AE(%d), AR(%d)\nInit Params:", en_al, en_af, en_ae, en_ar);
    CamParams init_params{
            cam_conf.init_et,
            cam_conf.init_eg,
            cam_conf.init_intensities,
            cam_conf.init_pos};
    init_params.Print();

    /* Determine whether to enable heatmap */
    bool enable_hmap;
    if (cam_conf.roi.area() <= 0) {
        enable_hmap = enable_hmap_;
    } else {
        enable_hmap = false;
        printf("ROI is set to (%f, %f, %f, %f), heatmap is disabled.\n",
               cam_conf.roi.x, cam_conf.roi.y, cam_conf.roi.width, cam_conf.roi.height);
    }

    /* Initialize submodules configuration */
    LoadCamConf(cam_conf);
    ar_obj.Init(barcode_wrapper);
    pipeline.clear();

    if (en_ae | en_af) {
        ae_obj.Init(ae_conf, en_al, enable_hmap);
        pipeline.emplace_back(AEQT);
    } else {
        ae_obj.params_next.exp_time = cam_conf.init_et;
        ae_obj.params_next.exp_gain = cam_conf.init_eg;
        ae_obj.params_next.lights = cam_conf.init_intensities;
    }

    if (en_af) {
        af_obj.Init(af_conf, enable_hmap);
        pipeline.emplace_back(AF);
    } else {
        af_obj.next_pos = cam_conf.init_pos;
    }

    if (en_ae) {
        ae_obj.Init(ae_conf, en_al, enable_hmap);
        pipeline.emplace_back(AEST);
    } else {
        ae_obj.params_next.exp_time = ae_conf.init_et;
        ae_obj.params_next.exp_gain = ae_conf.init_eg;
        ae_obj.params_next.lights = ae_conf.init_intensities;
    }

    if (en_ar) {
        pipeline.emplace_back(AR);
    }

    /* Append END to the pipeline by default */
    pipeline.emplace_back(END);
    phase = pipeline.begin();

    next_params.exp_time = ae_obj.params_next.exp_time;
    next_params.exp_gain = ae_obj.params_next.exp_gain;
    next_params.lights = ae_obj.params_next.lights;
    next_params.focus_pos = af_obj.next_pos;
    best_params = next_params;

    printf("[AT4VS] Initialization Done\nNext Params:");
    next_params.Print();
}

void AT4VsImpl::LoadCamConf(CamConf &cam_conf) {
    /* Load AF configuration */
    af_conf.START_POS = cam_conf.start_pos;
    af_conf.END_POS = cam_conf.end_pos;
    af_conf.IMG_WIDTH = cam_conf.img_width;
    af_conf.IMG_HEIGHT = cam_conf.img_height;
    af_conf.ROI = cam_conf.roi;
    af_conf.INIT_POS = cam_conf.init_pos;
    /* Load AE configuration */
    ae_conf.ae_mode = cam_conf.ae_mode;
    ae_conf.min_et = cam_conf.min_et;
    ae_conf.max_et = cam_conf.max_et;
    ae_conf.min_eg = cam_conf.min_eg;
    ae_conf.max_eg = cam_conf.max_eg;
    ae_conf.eg_quant_scale = cam_conf.eg_quant_scale;
    ae_conf.max_lt = cam_conf.max_lt;
    ae_conf.min_intensity = cam_conf.min_intensity;
    ae_conf.max_intensity = cam_conf.max_intensity;
    ae_conf.roi = cam_conf.roi;
    ae_conf.init_et = cam_conf.init_et;
    ae_conf.init_eg = cam_conf.init_eg;
    ae_conf.init_intensities = cam_conf.init_intensities;
}

void AT4VsImpl::SequentialExec(const cv::Mat &image) {
    switch (*phase) {
        case AEQT: {
            printf("[AT4VS] AEQT Running: \n");
            ae_obj.QuickTune(image);
            break;
        }
        case AEST: {
            printf("[AT4VS] AEST Running: \n");
            if (ae_obj.enable_hmap) {
                cv::Mat hmap = hmap_obj.Infer(image);
                ae_obj.StepTune(image, hmap);
            } else {
                ae_obj.StepTune(image);
            }
            break;
        }
        case AF: {
            printf("[AT4VS] AF Running: \n");
            if (af_obj.enable_hmap) {
                cv::Mat hmap = hmap_obj.Infer(image);
                af_obj.Run(image, hmap);
            } else {
                af_obj.Run(image);
            }
            break;
        }
        case AR: {
            printf("[AT4VS] AR Running");
            ar_obj.Decode(image, ar_params);
            break;
        }
        case END: {
            printf("[AT4VS] END\n");
            break;
        }
        default: {
            fprintf(stderr, "Invalid phase!\n");
            break;
        }
    }
}

void AT4VsImpl::UpdateNextParams() {
    switch (*phase) {
        case AEQT: {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            if (ae_obj.end_qt) {
                printf("[AT4VS] AEQT Done: exp-time=%d, exp-gain=%d, lights=%d,%d,%d,%d\n\n",
                       ae_obj.params_next.exp_time, ae_obj.params_next.exp_gain,
                       ae_obj.params_next.lights[0], ae_obj.params_next.lights[1],
                       ae_obj.params_next.lights[2], ae_obj.params_next.lights[3]);
                *phase++; // Move to the next phase
            }
            break;
        }
        case AEST: {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            if (ae_obj.end_st) {
                printf("[AT4VS] AEST Done: exp-time=%d, exp-gain=%d, lights=%d,%d,%d,%d\n\n",
                       ae_obj.params_best.exp_time, ae_obj.params_best.exp_gain,
                       ae_obj.params_best.lights[0], ae_obj.params_best.lights[1],
                       ae_obj.params_best.lights[2], ae_obj.params_best.lights[3]);
                best_params.lights = ae_obj.params_best.lights;
                best_params.exp_time = ae_obj.params_best.exp_time;
                best_params.exp_gain = ae_obj.params_best.exp_gain;
                *phase++; // Move to the next phase
            }
            break;
        }
        case AF: {
            next_params.focus_pos = af_obj.next_pos;
            if (af_obj.end_iter) {
                printf("[AT4VS] AF Done: focus-pos=%d\n\n", af_obj.best_pos);
                best_params.focus_pos = af_obj.best_pos;
                *phase++; // Move to the next phase
            }
            break;
        }
        case AR: {
            printf("[AT4VS] AR Done\n\n");
            if (!ar_params.read_1D && !ar_params.read_2D) {
                ar_obj.ResetOriParams();
            }
            *phase++; // Move to the next phase
            break;
        }
        case END: {
            printf("[AT4VS] END\n\n");
            break;
        }
        default: {
            fprintf(stderr, "Invalid phase!\n");
            break;
        }
    }
}
