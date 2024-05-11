/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "at_vs_impl.h"

// #ifdef USE_TENGINE
// #include "hmap_infer_tengine.h"
// #elif USE_NVTAI
// // TODO: Add NVTAI implementation
// #elif USE_RKNN
// #include "hmap_infer_rknn.h"
// #endif


AT4VsImpl::AT4VsImpl(bool enable_hmap) {
    this->at_roi = cv::Rect(0, 0, 0, 0);
    this->code_regions.clear();
    this->run_decode_finish = false;

    this->ae_target_brt = {64, 32, 96, 16, 128};
    this->ae_target_brt_idx = 0;

    this->refine_code_brt = {64, 32};
    this->refine_code_brt_idx = 0;
    this->refine_ae_finish = false;

    this->af_take_onemore = false;
    this->af_take_onemore_finish = false;
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
    this->pipeline.clear();

    this->barcode_wrapper_ = &barcode_wrapper;
    this->enable_al = en_al;

    if (en_ae | en_af) {
        ae_obj.Init(ae_conf, en_al, enable_hmap);
        this->pipeline.emplace_back(ATPhase::AEQT);
    } else {
        ae_obj.params_next.exp_time = cam_conf.init_et;
        ae_obj.params_next.exp_gain = cam_conf.init_eg;
        ae_obj.params_next.lights = cam_conf.init_intensities;
    }

    if (en_af) {
        af_obj.Init(af_conf, enable_hmap);
        this->pipeline.emplace_back(ATPhase::AF);
    } else {
        af_obj.next_pos = cam_conf.init_pos;
    }

    if (en_ae) {
        ae_obj.Init(ae_conf, en_al, enable_hmap);
        this->pipeline.emplace_back(ATPhase::AEST);
        this->pipeline.emplace_back(ATPhase::REFINE);
    } else {
        ae_obj.params_next.exp_time = ae_conf.init_et;
        ae_obj.params_next.exp_gain = ae_conf.init_eg;
        ae_obj.params_next.lights = ae_conf.init_intensities;
    }

    if (en_ar) {
        this->pipeline.emplace_back(ATPhase::AR);
    }

    /* Append END to the pipeline by default */
    this->pipeline.emplace_back(ATPhase::END);
    phase = this->pipeline.begin();

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

void AT4VsImpl::SetRoi(const cv::Rect &roi)
{
    this->at_roi = roi;
}

void AT4VsImpl::SequentialExec(const cv::Mat &image) {
    switch (*phase) {
        case ATPhase::AEQT: {
            printf("[AT4VS] AEQT Running: \n");
            std::vector<cv::Rect> ae_rois;
            ae_rois.clear();
            if(this->at_roi.width > 0)
            {
                ae_rois.emplace_back(this->at_roi);
            }

            for(auto &region : ae_rois)
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

            int target_brt = this->ae_target_brt[this->ae_target_brt_idx];  // 第一次的target brightness，默认是64
            ae_obj.QuickTune(image, target_brt, ae_rois);
            break;
        }
        case ATPhase::AF: {
            printf("[AT4VS] AF Running: \n");
            
            if(af_obj.end_iter && this->af_take_onemore)
            {
                std::cout << "AF take onemore" << std::endl;
                this->af_take_onemore = false;
                this->af_take_onemore_finish = true;
                this->cached_image = image;
                break;
            }

            // 当跑完第一阶段的大致对焦后，start_fit会置位，此时会跑一次解码算法，得到code_regions，之后fit阶段就会只关注code_regions的区域
            if(af_obj.start_fit && !this->run_decode_finish)
            {
                std::cout << "af phase running decode" << std::endl;
                // 如果没有定位到码，就不需要跑后面的refine了，会在UpdateNextParams()里更新
                this->code_regions = this->barcode_wrapper_->Decode(image, this->ar_info);  
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

                // 如果解码函数没有定位到码，但上位机设置了roi的话，就使用上位机的roi作为refine的区域
                if(this->code_regions.empty() && this->at_roi.width > 0)
                {
                    this->code_regions.emplace_back(this->at_roi);
                }

                std::cout << "code_regions " << std::endl;
                for(auto ele : this->code_regions)
                {
                    std::cout << ele << std::endl;
                }

                // if(this->code_regions.empty())
                // {
                //     this->*phase--;
                // }
                this->run_decode_finish = true;
            }

            if(!af_obj.start_fit)  // 还没有进入fit阶段，说明还没有跑解码函数，此时使用sdk本身设置的roi
            {
                std::vector<cv::Rect> af_rois;
                af_rois.clear();
                if(this->at_roi.width > 0)
                {
                    af_rois.emplace_back(this->at_roi);
                }

                for(auto &region : af_rois)
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

                af_obj.Run(image, af_rois);  
            }
            else                   // 进入fit阶段，使用decode函数的结果
            {
                af_obj.Run(image, this->code_regions);
            }

            break;
        }
        case ATPhase::AEST: {
            printf("[AT4VS] AEST Running: \n");
            this->cached_image = image;
            int target_brt = this->ae_target_brt[this->ae_target_brt_idx];
            std::cout << "target_brt " << target_brt << std::endl;
            ae_obj.QuickTune(image, target_brt);

            // ae_obj.StepTune(image);
            break;
        }
        case ATPhase::REFINE:{
            printf("[AT4VS] REFINE Running: \n");
            this->cached_image = image;

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

            if(!this->refine_ae_finish)
            {
                std::cout << "refine ae running" << std::endl;
                int target_brt = this->refine_code_brt[this->refine_code_brt_idx];
                this->ae_obj.QuickTune(image, target_brt, this->code_regions);
            }
            else
            {
                std::cout << "refine af running" << std::endl;
                this->af_obj.Run(image, this->code_regions);
            }
            break;
        }
        case ATPhase::AR: {
            printf("[AT4VS] AR Running");
            this->barcode_wrapper_->Decode(image, this->ar_info);
            break;
        }
        case ATPhase::END: {
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
        case ATPhase::AEQT: {
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
            }
            if (ae_obj.ae_fail)
            {
                std::cout << "aeqt fail, pipeline end" << std::endl;
                this->phase = pipeline.end()-1;
            }

            break;
        }
        case ATPhase::AEST: {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            next_params.focus_pos = best_params.focus_pos;
            
            if(this->ae_obj.end_qt)
            {
                std::cout << "UpdateNextParams phase==aest, run decode()" << std::endl;
                this->code_regions = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info);
                
                // std::string save_name = std::to_string(this->ae_target_brt[this->ae_target_brt_idx]) + ".png";
                // cv::imwrite(save_name, this->cached_image);
                std::cout << "code_regions " << std::endl;
                for(auto ele : this->code_regions)
                {
                    std::cout << ele << std::endl;
                }

                if(!this->ar_info.successful_code_type.empty())   // 成功解出至少一个码
                {
                    printf("[AT4VS] AEST Done: exp-time=%d, exp-gain=%d, lights=%d,%d,%d,%d\n\n",
                        ae_obj.params_best.exp_time, ae_obj.params_best.exp_gain,
                        ae_obj.params_best.lights[0], ae_obj.params_best.lights[1],
                        ae_obj.params_best.lights[2], ae_obj.params_best.lights[3]);
                    best_params.lights = ae_obj.params_best.lights;
                    best_params.exp_time = ae_obj.params_best.exp_time;
                    best_params.exp_gain = ae_obj.params_best.exp_gain;
                    *phase++; // Move to the next phase, 下一个阶段是refine, refine阶段不会改变灯光

                    this->ae_obj.end_qt = false;
                    af_obj.end_iter = false;
                }
                else      // 没有解出码的
                {
                    this->ae_obj.end_qt = false;
                    this->ae_target_brt_idx += 1;
                    if(this->ae_target_brt_idx == this->ae_target_brt.size()) // 当前亮度都轮询完之后，更换灯光后重新轮询亮度
                    {
                        std::cout << "AT4VsImpl::UpdateNextParams() AEST updatelights() " << std::endl;
                        this->ae_target_brt_idx = 0;
                        this->ae_obj.UpdateLights();   // 如果一直都没有解出码，就会导致ae_obj.ae_fail，就会跳出at
                        next_params.lights = ae_obj.params_next.lights;
                        next_params.exp_gain = ae_obj.params_next.exp_gain;

                    }
                    std::cout << "aest no code detected, next brt " << this->ae_target_brt[this->ae_target_brt_idx] << std::endl;
                }

            }
            if (ae_obj.ae_fail)
            {
                std::cout << "aest fail, pipeline end" << std::endl;
                this->phase = pipeline.end()-1;
            }

            break;
        }
        case ATPhase::REFINE: {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            next_params.focus_pos = af_obj.next_pos;
    
            if(ae_obj.end_qt)
            {
                this->ae_obj.end_qt = false;
                auto refine_ae_code_region = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info);

                if(!this->ar_info.successful_code_type.empty())   // 在调整亮度后，如果能解到码，则直接进入对焦refine
                {
                    printf("[AT4VS] REFINE AE Done: exp-time=%d, exp-gain=%d, lights=%d,%d,%d,%d\n\n",
                        ae_obj.params_best.exp_time, ae_obj.params_best.exp_gain,
                        ae_obj.params_best.lights[0], ae_obj.params_best.lights[1],
                        ae_obj.params_best.lights[2], ae_obj.params_best.lights[3]);
                    best_params.lights = ae_obj.params_best.lights;
                    best_params.exp_time = ae_obj.params_best.exp_time;
                    best_params.exp_gain = ae_obj.params_best.exp_gain;

                    this->refine_ae_finish = true;
                    this->af_obj.ResetSamples();

                    next_params.focus_pos = af_obj.next_pos;

                }
                else   // 调整亮度后如果解不了码，就用下一个亮度，如果设定的所有亮度都解不了码，就用回AEST成功时的曝光增益，只refine焦距
                {
                    this->refine_code_brt_idx += 1;
                    if(this->refine_code_brt_idx == this->refine_code_brt.size())
                    {
                        this->refine_ae_finish = true;
                        this->af_obj.ResetSamples();

                        next_params.exp_time = best_params.exp_time;
                        next_params.exp_gain = best_params.exp_gain;
                        next_params.focus_pos = af_obj.next_pos;
                    }
                }
            }
            if(af_obj.end_iter)
            {
                std::cout << "[AT4VS] REFINE AF Done" << std::endl;
                best_params.focus_pos = af_obj.best_pos;
                *phase++;
            }

            if (ae_obj.ae_fail)
            {
                std::cout << "aest fail, pipeline end" << std::endl;
                this->phase = pipeline.end()-1;
            }

            break;
        }
        case ATPhase::AF: {
            next_params.focus_pos = af_obj.next_pos;
            // af完了的时候，需要跑一次解码，根据是否定位到码来决定是否还需要调整曝光
            if (af_obj.end_iter && !this->af_take_onemore_finish) {
                printf("[AT4VS] AF Done: focus-pos=%d\n\n", af_obj.best_pos);

                next_params.focus_pos = af_obj.best_pos;
                best_params.focus_pos = af_obj.best_pos;
                af_obj.next_pos = af_obj.best_pos;

                this->af_take_onemore = true;
            }

            if (af_obj.end_iter && this->af_take_onemore_finish)
            {
                this->af_take_onemore_finish = false;
                std::cout << "UpdateNextParams phase==af, run decode()" << std::endl;
                std::vector<cv::Rect> af_finish_decode_region = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info);
                std::cout << "af_finish_decode_region.size " << af_finish_decode_region.size() << std::endl; 
                for(auto ele : af_finish_decode_region)
                {
                    std::cout << ele << std::endl;
                }
                
                if(this->ar_info.successful_code_type.empty())//如果没解到码，就看有没有打开ae，打开了的话，就跳转到AEST
                {
                    this->ae_obj.end_qt = false;
                    this->ae_target_brt_idx = (this->ae_target_brt_idx + 1) % this->ae_target_brt.size();
                    auto it = std::find(this->pipeline.begin(), this->pipeline.end(), ATPhase::AEST);
                    if(it != this->pipeline.end())   // 打开了ae，则跳到aest
                    {
                        std::cout << "af finish and no code detected, phase = aest" << std::endl;
                        auto phase_idx = std::distance(this->pipeline.begin(), it);
                        this->phase = this->pipeline.begin() + phase_idx;
                    }
                    else     // 没开ae，跳到下一个，ar 或 end
                    {
                        std::cout << "af finish and no code detected, phase++ " << std::endl;
                        *phase++; // Move to the next phase
                    }
                }
                else    // 如果解了码的话，开了ae就跳过aest,由于aest默认有refine，所有下面是跳过refine，没开ae就自动跳到下一个
                {
                    // auto it = std::find(this->pipeline.begin(), this->pipeline.end(), ATPhase::REFINE);
                    // if(it != this->pipeline.end())
                    // {
                    //     std::cout << "af finish and code detected, phase = refine" << std::endl;
                    //     auto phase_idx = std::distance(this->pipeline.begin(), it);
                    //     this->phase = this->pipeline.begin() + phase_idx;
                    // }
                    std::cout << "af finish and code detected, phase++" << std::endl;
                    *phase++; // Move to the next phase
                }
            }
            break;
        }
        case ATPhase::AR: {
            printf("[AT4VS] AR Done\n\n");
            *phase++; // Move to the next phase
            break;
        }
        case ATPhase::END: {
            printf("[AT4VS] END\n\n");
            break;
        }
        default: {
            fprintf(stderr, "Invalid phase!\n");
            break;
        }
    }
}
