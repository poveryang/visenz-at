#include "at_vs_impl.h"

#ifdef USE_TENGINE
#include "hmap_infer_tengine.h"
#elif USE_NVTAI
// TODO: Add NVTAI implementation
#elif USE_RKNN
#include "hmap_infer_rknn.h"
#endif


AT4VsImpl::AT4VsImpl(bool enable_hmap) {
    /* Load heat-map generator if enabled */
    enable_hmap_ = enable_hmap;
    if (enable_hmap_) {
        printf("[AT4VS] Heatmap is enabled\n");
#ifdef USE_TENGINE
        hmap_obj = std::make_shared<HMapInferTengine>();
        hmap_obj->SetInferSize(cv::Size2i(1920, 1200));  // please get size from corresponding device
        hmap_obj->Init("/usr/scanner/algorithm/hmap-v7-qat-tmp-uint8-2.tmfile");
#elif USE_NVTAI
        std::string model_path = "/usr/scanner/algorithm/nvt_model.bin";
        int input_width = 1280;
        int input_height = 800;
        hmap_obj = std::make_shared<NvtAIInferenceEngine>(model_path, input_width, input_height);
        hmap_obj->Init();
        hmap_obj->LoadModel();  // TODO: delay loading model after camera is initialized
#elif USE_RKNN
        printf("[AT4VS] Using RKNN for heatmap inference\n");
        hmap_obj = std::make_shared<HMapInferRK>();
        hmap_obj->SetInferSize(cv::Size2i(1920, 1200));  // please get size from corresponding device
        hmap_obj->Init("/usr/scanner/algorithm/hmap-v2.rknn");
#else
        printf("[AT4VS] Heatmap is enabled but no inference engine is specified\n");
        enable_hmap_ = false;
#endif
    }

    this->code_regions.clear();
    this->sdk_rois.clear();
    this->run_decode_finish = false;

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
    // ar_obj.Init(barcode_wrapper);
    pipeline.clear();

    this->barcode_wrapper_ = &barcode_wrapper;
    this->sdk_rois = this->barcode_wrapper_->GetSdkRois();

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
    ae_conf.img_width = cam_conf.img_width;
    ae_conf.img_height = cam_conf.img_height;
    ae_conf.init_et = cam_conf.init_et;
    ae_conf.init_eg = cam_conf.init_eg;
    ae_conf.init_intensities = cam_conf.init_intensities;
}

void AT4VsImpl::SequentialExec(const cv::Mat &image) {
    switch (*phase) {
        case AEQT: {
            printf("[AT4VS] AEQT Running: \n");
            ae_obj.QuickTune(image, 64);
            break;
        }
        case AEST: {
            printf("[AT4VS] AEST Running: \n");
            if (ae_obj.enable_hmap) {
                cv::Mat hmap = hmap_obj->Inference(image);
                ae_obj.StepTune(image, hmap);
            } else {
                ae_obj.StepTune(image);
            }
            break;
        }
        case AF: {
            printf("[AT4VS] AF Running: \n");
            // if (af_obj.enable_hmap) {
            //     cv::Mat hmap = hmap_obj->Inference(image);
            //     af_obj.Run(image, hmap);
            // } else {
            //     af_obj.Run(image);
            // }

            if(!af_obj.start_fit)  // 还没有进入fit阶段，说明还没有跑解码函数，此时使用sdk本身设置的roi
            {
                af_obj.Run(image, this->sdk_rois);  
            }
            else                   // 进入fit阶段，使用decode函数的结果
            {
                af_obj.Run(image, this->code_regions);
            }

            // 当跑完第一阶段的大致对焦后，start_fit会置位，此时会跑一次解码算法，得到code_regions，之后fit阶段就会只关注code_regions的区域
            if(af_obj.start_fit && !this->run_decode_finish)
            {
                // 如果没有定位到码，就不需要跑后面的refine了，会在UpdateNextParams()里更新
                this->code_regions = this->barcode_wrapper_->Decode(image);  
                for(auto &region : this->code_regions)
                {
                    // 防止越界
                    region.x = std::max(0, region.x);
                    region.y = std::max(0, region.y);
                    if(region.br().x > image.cols-1)
                    {
                        region.width = image.cols - region.x;
                    }
                    if(region.br().y > image.rows-1)
                    {
                        region.height = image.rows - region.y;
                    }
                }
                this->run_decode_finish = true;
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
                best_params.lights = ae_obj.params_best.lights;
                best_params.exp_time = ae_obj.params_best.exp_time;
                best_params.exp_gain = ae_obj.params_best.exp_gain;
                *phase++; // Move to the next phase
                this->timer_start_ = std::chrono::high_resolution_clock::now();
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
                auto timer_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timer_end-this->timer_start_);
                std::cout << "first stage af time: " << duration.count() / 1000 << "  ms" << std::endl;

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
