/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "at_vs_impl.h"

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
//  发版记得在cmake中更新版本，重新编译后再发布
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

AT4VsImpl::AT4VsImpl(bool enable_hmap) 
{
    this->ae_target_brt = {64, 32, 16, 8, 96, 128};
    this->refine_code_brt = {96, 64, 32};
}

void AT4VsImpl::Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
                     bool en_al, bool en_af, bool en_ae, bool en_ar) 
{
    printf("[AT4VS] Initializing: \nAL(%d), AF(%d), AE(%d), AR(%d)\nInit Params:", en_al, en_af, en_ae, en_ar);
    CamParams init_params{cam_conf.init_et, cam_conf.init_eg, cam_conf.init_intensities, cam_conf.init_pos};
    #ifdef BUILD_WITH_LOG
        init_params.Print();
    #endif

    this->at_roi = cv::Rect(0, 0, 0, 0);
    this->code_regions.clear();

    this->ae_target_brt_idx = 0;
    this->run_decode_finish = false;
    this->cached_image = cv::Mat();
    this->refine_code_brt_idx = 0;
    this->refine_ae_finish = false;
    this->af_take_onemore = false;
    this->af_take_onemore_finish = false;

    this->base_lights.clear();
    this->base_exp_time = 1000;
    this->base_exp_gain = 32;
    this->set_base = false;

    #ifdef BUILD_WITH_LOG
        std::cout << "ae_target_brt ";
        for(auto ele : this->ae_target_brt)
        {
            std::cout << ele << ", ";
        }
        std::cout << std::endl;
        std::cout << "ae_target_brt_idx " << this->ae_target_brt_idx << std::endl;
    #endif

    /* Initialize submodules configuration */
    LoadCamConf(cam_conf);

    #ifdef BUILD_WITH_LOG
        std::cout << "init load cam conf finish" << std::endl;
    #endif

    this->pipeline.clear();
    this->barcode_wrapper_ = &barcode_wrapper;
    #ifdef BUILD_WITH_LOG
        std::cout << "set barcode_wrapper_ done" << std::endl;
    #endif
    
    this->enable_al = en_al;
    this->enable_ae = en_ae;
    this->enable_af = en_af;

    if (en_ae | en_af) 
    {
        ae_obj.Init(ae_conf, en_al);
        this->pipeline.emplace_back(ATPhase::AEQT);
        #ifdef BUILD_WITH_LOG
            std::cout << "set aeqt done" << std::endl;
        #endif
    } 
    else 
    {
        ae_obj.params_next.exp_time = cam_conf.init_et;
        ae_obj.params_next.exp_gain = cam_conf.init_eg;
        ae_obj.params_next.lights = cam_conf.init_intensities;
    }
    
    if (en_af) 
    {
        af_obj.Init(af_conf);  // 执行af_obj.Init时，会一路进到af_impl的构造函数，初始化 af_obj.next_pos = 1，不会使用设置的初始值
        this->pipeline.emplace_back(ATPhase::AF);
        #ifdef BUILD_WITH_LOG
            std::cout << "set af done" << std::endl;
        #endif
    } 
    else 
    {
        af_obj.next_pos = cam_conf.init_pos;
    }

    if (en_ae) 
    {
        ae_obj.Init(ae_conf, en_al);
        this->pipeline.emplace_back(ATPhase::AEST);
        this->pipeline.emplace_back(ATPhase::REFINE);
        #ifdef BUILD_WITH_LOG
            std::cout << "set refine done" << std::endl;
        #endif
    } 
    else 
    {
        ae_obj.params_next.exp_time = ae_conf.init_et;
        ae_obj.params_next.exp_gain = ae_conf.init_eg;
        ae_obj.params_next.lights = ae_conf.init_intensities;
    }

    if (en_ar) 
    {
        this->pipeline.emplace_back(ATPhase::AR);
        #ifdef BUILD_WITH_LOG
            std::cout << "set ar done" << std::endl;
        #endif
    }

    /* Append END to the pipeline by default */
    this->pipeline.emplace_back(ATPhase::END);
    phase = this->pipeline.begin();
    #ifdef BUILD_WITH_LOG
        std::cout << "set pipeline done" << std::endl;
    #endif

    next_params.exp_time = ae_obj.params_next.exp_time;
    next_params.exp_gain = ae_obj.params_next.exp_gain;
    next_params.lights = ae_obj.params_next.lights;
    next_params.focus_pos = af_obj.next_pos;
    best_params = next_params;

    #ifdef BUILD_WITH_LOG
        printf("[AT4VS] Initialization Done\nNext Params:");
        next_params.Print();
    #endif
}

void AT4VsImpl::LoadCamConf(CamConf &cam_conf) 
{
    /* Load AF configuration */
    af_conf.start_pos = cam_conf.start_pos;
    af_conf.end_pos = cam_conf.end_pos;
    af_conf.coarse_step = cam_conf.coarse_af_step;
    af_conf.refine_step = cam_conf.refine_af_step;

    /* Load AE configuration */
    ae_conf.ae_mode = cam_conf.ae_mode;
    ae_conf.min_et = cam_conf.min_et;
    ae_conf.max_et = cam_conf.max_et;
    ae_conf.min_eg = cam_conf.min_eg;
    ae_conf.max_eg = cam_conf.max_eg;
    ae_conf.min_intensity = cam_conf.min_intensity;
    ae_conf.max_intensity = cam_conf.max_intensity;
    ae_conf.init_et = cam_conf.init_et;
    ae_conf.init_eg = cam_conf.init_eg;
    ae_conf.init_intensities = cam_conf.init_intensities;
    // ae_conf.init_intensities = {24};

    #ifdef BUILD_WITH_LOG
        std::cout << "========= input camera conf: " << std::endl;
        std::cout << "    af_conf.start_pos: " << af_conf.start_pos << std::endl;
        std::cout << "    af_conf.end_pos: " << af_conf.end_pos << std::endl;
        std::cout << "    af_conf.coarse_step: " << af_conf.coarse_step << std::endl;
        std::cout << "    af_conf.refine_step: " << af_conf.refine_step << std::endl;
        std::cout << std::endl;
        std::cout << "    ae_conf.ae_mode: " << ae_conf.ae_mode << std::endl;
        std::cout << "    ae_conf.min_et: " << ae_conf.min_et << std::endl;
        std::cout << "    ae_conf.max_et: " << ae_conf.max_et << std::endl;
        std::cout << "    ae_conf.min_eg: " << ae_conf.min_eg << std::endl;
        std::cout << "    ae_conf.max_eg: " << ae_conf.max_eg << std::endl;
        std::cout << "    ae_conf.min_intensity: " << ae_conf.min_intensity << std::endl;
        std::cout << "    ae_conf.max_intensity: " << ae_conf.max_intensity << std::endl;
        std::cout << "    ae_conf.init_et: " << ae_conf.init_et << std::endl;
        std::cout << "    ae_conf.init_eg: " << ae_conf.init_eg << std::endl;
        std::cout << "    ae_conf.init_intensities: [";
        for(auto ele : ae_conf.init_intensities)
        {
            std::cout << ele << ", ";
        }
        std::cout << "]" << std::endl;
        std::cout << "=====================================" << std::endl;
    #endif
}

void AT4VsImpl::SetRoi(const cv::Rect &roi)
{
    this->at_roi = roi;
}

void AT4VsImpl::SequentialExec(const cv::Mat &image) 
{
    switch (*phase) 
    {
        case ATPhase::AEQT: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AEQT Running: \n");
            #endif
            std::vector<cv::Rect> ae_rois;
            ae_rois.clear();
            if(this->at_roi.width > 0)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "exec aeqt using setting roi " << this->at_roi << std::endl;
                #endif
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
            #ifdef BUILD_WITH_LOG
                std::cout << "aeqt exec this->ae_target_brt_idx " << this->ae_target_brt_idx << std::endl;
            #endif
            int target_brt = this->ae_target_brt[this->ae_target_brt_idx];  // 第一次的target brightness，默认是64
            #ifdef BUILD_WITH_LOG
                    std::cout << "exec aeqt code_regions " << std::endl;
                    for(auto ele : ae_rois)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
            
            int target_thre = std::min(15, int(target_brt/2));
            ae_obj.QuickTune(image, target_brt, ae_rois, target_thre, false);
            break;
        }
        case ATPhase::AF: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AF Running: \n");
            #endif      
            if(af_obj.end_iter && this->af_take_onemore)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "AF take onemore" << std::endl;
                #endif
                this->af_take_onemore = false;
                this->af_take_onemore_finish = true;
                this->cached_image = image;
                break;
            }

            // 当跑完第一阶段的大致对焦后，start_fit会置位，此时会跑一次解码算法，得到code_regions，之后fit阶段就会只关注code_regions的区域
            if(af_obj.start_fit && !this->run_decode_finish)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "af phase running decode" << std::endl;
                #endif
                // 如果没有定位到码，就不需要跑后面的refine了，会在UpdateNextParams()里更新
                auto tmp_code_regions = this->barcode_wrapper_->Decode(image, this->ar_info, this->at_roi);
                if (!tmp_code_regions.empty())
                {
                    this->code_regions = tmp_code_regions;
                }
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
                    #ifdef BUILD_WITH_LOG
                        std::cout << "af coarse phase finish, no code detected, using setting roi " << this->at_roi << std::endl;
                    #endif
                    this->code_regions.emplace_back(this->at_roi);
                }
                
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

                #ifdef BUILD_WITH_LOG
                    std::cout << "af coarse phase using roi " << std::endl;
                    for(auto ele : af_rois)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                this->af_obj.Run(image, af_rois);  
            }
            else                   // 进入fit阶段，使用decode函数的结果
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "exec fit stage af code_regions " << std::endl;
                    for(auto ele : this->code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                this->af_obj.Run(image, this->code_regions);
            }

            break;
        }
        case ATPhase::AEST: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AEST Running: \n");
            #endif
            this->cached_image = image;
            int target_brt = this->ae_target_brt[this->ae_target_brt_idx];
            #ifdef BUILD_WITH_LOG
                std::cout << "target_brt " << target_brt << std::endl;
            #endif

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
            #ifdef BUILD_WITH_LOG
                std::cout << "exec aest code region :" << std::endl;
                for(const auto &roi : this->code_regions)
                {
                    std::cout << "    " << roi << std::endl;
                }
                std::cout << std::endl;
            #endif

            int target_thre = std::min(15, int(target_brt/2));
            ae_obj.QuickTune(image, target_brt, this->code_regions, target_thre, true);

            break;
        }
        case ATPhase::REFINE:
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] REFINE Running: \n");
            #endif
            this->cached_image = image;
            #ifdef BUILD_WITH_LOG
                std::cout << "refine_ae_finish " << this->refine_ae_finish << std::endl;
            #endif

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

            if(!this->refine_ae_finish)
            {
                int target_brt = this->refine_code_brt[this->refine_code_brt_idx];
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine ae running, target_brt: " << target_brt << std::endl;
                    std::cout << "exec refine aeobj code region :" << std::endl;
                    for(const auto &roi : this->code_regions)
                    {
                        std::cout << "    " << roi << std::endl;
                    }
                    std::cout << std::endl;
                #endif

                int target_thre = std::min(10, int(target_brt/2));
                this->ae_obj.QuickTune(image, target_brt, this->code_regions, target_thre, true);
            }
            else
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine af running" << std::endl;
                    std::cout << "exec refine afobj code region :" << std::endl;
                    for(const auto &roi : this->code_regions)
                    {
                        std::cout << "    " << roi << std::endl;
                    }
                    std::cout << std::endl;
                #endif

                this->af_obj.Run(image, this->code_regions);
            }
            break;
        }
        case ATPhase::AR: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AR Running, run decode()");
            #endif
            auto tmp_code_regions = this->barcode_wrapper_->Decode(image, this->ar_info, this->at_roi);
            if(!tmp_code_regions.empty())
            {
                this->code_regions = tmp_code_regions;
            }
            break;
        }
        case ATPhase::END: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] END info \n");
                std::cout << "code regions size: " << this->code_regions.size() << std::endl;
                std::cout << "lights :[";
                for(auto ele : this->best_params.lights)
                {
                    std::cout << ele << ", ";
                }
                std::cout << "]" << std::endl;
            #endif

            // // 当AT结束，如果没有找到码，且 auto_light == true的情况下，打开所有灯光，把最终的输出曝光设置为AEQT时的曝光值
            // int lights_sum = 0;
            // for(auto ele : this->best_params.lights)
            // {
            //     lights_sum += ele;
            // }
            // if (this->enable_al && this->code_regions.empty() && lights_sum < 1)
            // {
            //     for(int i=0; i<this->best_params.lights.size(); i++)
            //     {
            //         this->best_params.lights[i] = 1;
            //     }
            //     this->best_params.exp_time = this->base_exp_time;
            //     this->best_params.exp_gain = this->base_exp_gain;
            //     #ifdef BUILD_WITH_LOG
            //         std::cout << "Cannot find code in the end, turn on all lights and set to aeqt's exp_time" << std::endl;
            //     #endif
            // }

            // 当AT结束，如果没有找到码，把最终的输出曝光设置为AEQT时的曝光值
            if (this->code_regions.empty())
            {
                this->best_params.lights = this->base_lights;
                this->best_params.exp_time = this->base_exp_time;
                this->best_params.exp_gain = this->base_exp_gain;
                #ifdef BUILD_WITH_LOG
                    std::cout << "Cannot find code in the end, turn on all lights and set to base params:" << std::endl;
                    std::cout << "exp_time " << this->best_params.exp_time << ", exp_gain " << this->best_params.exp_gain << std::endl;
                #endif
            }

            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] END\n");
            #endif

            break;
        }
        default: 
        {
            fprintf(stderr, "Invalid phase!\n");
            break;
        }
    }
}

void AT4VsImpl::UpdateNextParams() 
{
    switch (*phase) 
    {
        case ATPhase::AEQT: 
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "AT4VsImpl::UpdateNextParams() aeqt phase" << std::endl;
            #endif
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            if (ae_obj.end_qt) 
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "[AT4VS] AEQT Done: exp-time=" << ae_obj.params_next.exp_time << ", "
                              << "exp-gain=" << ae_obj.params_next.exp_gain << ", "
                              << "lights=[";
                    for(auto ele : ae_obj.params_next.lights)
                    {
                        std::cout << ele << ", ";
                    }
                    std::cout << "]" << std::endl;
                #endif
                best_params.lights = ae_obj.params_best.lights;
                best_params.exp_time = ae_obj.params_best.exp_time;
                best_params.exp_gain = ae_obj.params_best.exp_gain;
                *phase++; // Move to the next phase
            }
            if (ae_obj.ae_fail)
            {
                // std::cout << "aeqt fail, pipeline end" << std::endl;
                // this->phase = pipeline.end()-1;
                #ifdef BUILD_WITH_LOG
                    std::cout << "aeqt fail, can not reach given brightness, set current to best" << std::endl;
                    std::cout << "aeqt fial, set ae_fail = false" << std::endl;
                #endif
                ae_obj.ae_fail = false;
                this->ae_obj.ClearState(); //需要给后面的流程机会，aeqt用的是全图亮度，可能去不到目标亮度，但如果找到码区后，黑白比例更均匀，就有可能能达到目标亮度
                best_params.lights = ae_obj.params_next.lights;
                best_params.exp_time = ae_obj.params_next.exp_time;
                best_params.exp_gain = ae_obj.params_next.exp_gain;
                *phase++; // Move to the next phase
            }

            break;
        }
        case ATPhase::AF: 
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "AT4VsImpl::UpdateNextParams() af phase" << std::endl;
            #endif
            next_params.focus_pos = af_obj.next_pos;
            // af完了的时候，需要跑一次解码，根据是否定位到码来决定是否还需要调整曝光
            if (af_obj.end_iter && !this->af_take_onemore_finish) 
            {
                #ifdef BUILD_WITH_LOG
                    printf("[AT4VS] AF Done: focus-pos=%d\n\n", af_obj.best_pos);
                #endif

                next_params.focus_pos = af_obj.best_pos;
                best_params.focus_pos = af_obj.best_pos;
                af_obj.next_pos = af_obj.best_pos;

                this->af_take_onemore = true;
            }

            if (af_obj.end_iter && this->af_take_onemore_finish)
            {
                this->af_take_onemore_finish = false;
                #ifdef BUILD_WITH_LOG
                    std::cout << "UpdateNextParams phase==af, run decode()" << std::endl;
                    std::cout << "at_roi " << this->at_roi << std::endl;
                #endif
                auto tmp_code_regions = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info, this->at_roi);
                if(!tmp_code_regions.empty())
                {
                    this->code_regions  = tmp_code_regions;
                }
                #ifdef BUILD_WITH_LOG
                    std::cout << "this->code_regions.size " << this->code_regions.size() << std::endl; 
                    for(auto ele : this->code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                
                // if(this->ar_info.successful_code_type.empty())//如果没解到码，就看有没有打开ae，打开了的话，就跳转到AEST
                if(this->code_regions.empty())//如果没解到码，就看有没有打开ae，打开了的话，就跳转到AEST
                {
                    this->ae_obj.end_qt = false;
                    this->ae_target_brt_idx = (this->ae_target_brt_idx + 1) % this->ae_target_brt.size();
                    auto it = std::find(this->pipeline.begin(), this->pipeline.end(), ATPhase::AEST);
                    if(it != this->pipeline.end())   // 打开了ae，则跳到aest
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "af finish and no code detected, phase = aest" << std::endl;
                        #endif
                        auto phase_idx = std::distance(this->pipeline.begin(), it);
                        this->phase = this->pipeline.begin() + phase_idx;
                    }
                    else     // 没开ae，跳到下一个，ar 或 end
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "af finish and no code detected, phase++ " << std::endl;
                        #endif
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
                    #ifdef BUILD_WITH_LOG
                        std::cout << "af finish and code detected, phase++" << std::endl;
                    #endif
                    *phase++; // Move to the next phase
                }
            }
            break;
        }
        case ATPhase::AEST: 
        {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            next_params.focus_pos = best_params.focus_pos;
            
            #ifdef BUILD_WITH_LOG
                std::cout << "AT4VsImpl::UpdateNextParams() aest phase" << std::endl;
            #endif

            if(this->ae_obj.end_qt)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "aest ae_obj.end_qt=true, run decode()" << std::endl;
                #endif
                auto tmp_code_regions = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info, this->at_roi);
                // cv::imwrite("/usr/scanner/debug/at/aest_" + std::to_string(this->ae_target_brt[this->ae_target_brt_idx])+".png", this->cached_image);
                std::cout << "aest target brt " << this->ae_target_brt[this->ae_target_brt_idx] << ", code regions size " << tmp_code_regions.size() << std::endl;
                if(!tmp_code_regions.empty())
                {
                    this->code_regions = tmp_code_regions;
                }
                #ifdef BUILD_WITH_LOG
                    std::cout << "code_regions " << std::endl;
                    for(auto ele : this->code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif

                // 记录一组不太差的亮度参数，用于在无码场景时的最终结果
                // 这里选择的是aest时的第一个调整亮度，调整灯光后的不需要
                if(this->ae_target_brt_idx == 0 && !this->set_base)
                {
                    this->base_lights = ae_obj.params_next.lights;
                    this->base_exp_time = ae_obj.params_next.exp_time;
                    this->base_exp_gain = ae_obj.params_next.exp_gain;
                    this->set_base = true;
                    #ifdef BUILD_WITH_LOG
                        std::cout << "using aset target brt=" << this->ae_target_brt[0] << " as base param" << std::endl;
                    #endif
                }

                if(!this->code_regions.empty())   // 成功解出至少一个码
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "[AT4VS] AEST Done: exp-time=" << ae_obj.params_next.exp_time << ", "
                                  << "exp-gain=" << ae_obj.params_next.exp_gain << ", "
                                  << "lights=[";
                        for(auto ele : ae_obj.params_next.lights)
                        {
                            std::cout << ele << ", ";
                        }
                        std::cout << "]" << std::endl;
                    #endif
                    best_params.lights = ae_obj.params_best.lights;
                    best_params.exp_time = ae_obj.params_best.exp_time;
                    best_params.exp_gain = ae_obj.params_best.exp_gain;
                    *phase++; // Move to the next phase, 下一个阶段是refine, refine阶段不会改变灯光

                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest decode success, run refine" << std::endl;
                    #endif
                    this->ae_obj.end_qt = false;
                    // this->refine_ae_finish = true; // 如果aest的时候能解到码了，那refine阶段的ae就不跑了，以节省时间，这里可以实验看效果
                    if (this->enable_af)
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "af enable, turn on refine af" << std::endl;
                        #endif
                        this->af_obj.ResetSamples();
                    }
                }
                else      // 没有解出码的
                {
                    this->ae_obj.end_qt = false;
                    this->ae_target_brt_idx += 1;
                    if(this->ae_target_brt_idx == this->ae_target_brt.size()) // 当前亮度都轮询完之后，更换灯光后重新轮询亮度
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "AT4VsImpl::UpdateNextParams() AEST updatelights() " << std::endl;
                        #endif
                        this->ae_target_brt_idx = 0;
                        if(this->enable_al)
                        {
                            this->ae_obj.UpdateLights();   // 如果一直都没有解出码，就会导致ae_obj.ae_fail，就会跳出at
                        }
                        next_params.lights = ae_obj.params_next.lights;
                        next_params.exp_gain = ae_obj.params_next.exp_gain;

                    }
                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest no code detected, next brt " << this->ae_target_brt[this->ae_target_brt_idx] << std::endl;
                    #endif
                }
            }
            if (ae_obj.ae_fail)
            {
                // std::cout << "aest fail, pipeline end" << std::endl;
                // this->phase = pipeline.end()-1;
                #ifdef BUILD_WITH_LOG
                    std::cout << "aest fail, can not reach given brightness, set current to best" << std::endl;
                #endif
                auto tmp_code_regions = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info, this->at_roi); // aest fail的时候，也要更新code_region给后续使用
                if(!tmp_code_regions.empty())
                {
                    this->code_regions = tmp_code_regions;
                }
                
                #ifdef BUILD_WITH_LOG
                    std::cout << "aest fail, set ae_fail=false" << std::endl;
                #endif
                ae_obj.ae_fail = false;
                this->ae_obj.ClearState();
                best_params.lights = ae_obj.params_next.lights;
                best_params.exp_time = ae_obj.params_next.exp_time;
                best_params.exp_gain = ae_obj.params_next.exp_gain;
                *phase++; // Move to the next phase

                // 记录一组不太差的亮度参数，用于在无码场景时的最终结果
                // 这里用的是aest失败时，最接近时的值
                if(!this->set_base)
                {
                    this->base_lights = ae_obj.params_next.lights;
                    this->base_exp_time = ae_obj.params_next.exp_time;
                    this->base_exp_gain = ae_obj.params_next.exp_gain;
                    this->set_base = true;
                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest fail, using last try as base param" << std::endl;
                    #endif
                }
            }

            break;
        }
        case ATPhase::REFINE: 
        {
            next_params.lights = ae_obj.params_next.lights;
            next_params.exp_time = ae_obj.params_next.exp_time;
            next_params.exp_gain = ae_obj.params_next.exp_gain;
            next_params.focus_pos = af_obj.next_pos;

            #ifdef BUILD_WITH_LOG
                    std::cout << "AT4VsImpl::UpdateNextParams() refine phase" << std::endl;
            #endif
            // refine 阶段不会再调整灯光，只会调整曝光
            if(ae_obj.end_qt)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine phase: ae finish, run decode()" << std::endl;
                #endif
                this->ae_obj.end_qt = false;
                auto tmp_code_regions = this->barcode_wrapper_->Decode(this->cached_image, this->ar_info, this->at_roi);
                // cv::imwrite("/usr/scanner/debug/at/refine_" + std::to_string(this->refine_code_brt[this->refine_code_brt_idx])+".png", this->cached_image);
                std::cout << "refine ae target brt " << this->refine_code_brt[this->refine_code_brt_idx] << ", code regions size " << tmp_code_regions.size() << std::endl;
                if(!tmp_code_regions.empty())
                {
                    this->code_regions = tmp_code_regions;
                }

                if(!this->ar_info.successful_code_type.empty())   // 在调整亮度后，如果能解到码，则直接进入对焦refine
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "[AT4VS] REFINE AE Done: exp-time=" << ae_obj.params_next.exp_time << ", "
                                  << "exp-gain=" << ae_obj.params_next.exp_gain << ", "
                                  << "lights=[";
                        for(auto ele : ae_obj.params_next.lights)
                        {
                            std::cout << ele << ", ";
                        }
                        std::cout << "]" << std::endl;
                    #endif
                    best_params.lights = ae_obj.params_best.lights;
                    best_params.exp_time = ae_obj.params_best.exp_time;
                    best_params.exp_gain = ae_obj.params_best.exp_gain;

                    this->refine_ae_finish = true;

                    if (this->enable_af)
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "refine ae decode success and enable_af=True, run refine af" << std::endl;
                        #endif
                        this->af_obj.ResetSamples();   // 调用ResetSamples()才会使af_obj.end_iter=false
                        next_params.focus_pos = af_obj.next_pos;
                    }
                    else
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "refind ae decode success and enable_af=False, go to next phase" << std::endl;
                        #endif
                        *phase++;    // 如果没有打开af，如vs600，那么refine ae后直接就到下一阶段，跳过refine af
                    }

                }
                else   // 调整亮度后如果解不了码，就用下一个亮度，如果设定的所有亮度都解不了码，就用回AEST成功时的曝光增益，只refine焦距
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "refine decode empty" << std::endl;
                    #endif
                    this->refine_code_brt_idx += 1;
                    if(this->refine_code_brt_idx >= this->refine_code_brt.size())
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "refine ae decode fail ";
                        #endif
                        this->refine_ae_finish = true;
                        if (this->enable_af)           // 要打开了af才能进入 refine af
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "enable_af=True, run refine af" << std::endl;
                            #endif
                            this->af_obj.ResetSamples();
                        }
                        else 
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "enable_af=False, go to next phase" << std::endl;
                            #endif
                            *phase++;    // 如果没有打开af，如vs600，那么refine ae后直接就到下一阶段，跳过refine af
                        }
                        next_params.exp_time = best_params.exp_time;
                        next_params.exp_gain = best_params.exp_gain;
                        next_params.focus_pos = af_obj.next_pos;
                    }
                }
            }
            if (ae_obj.ae_fail)
            {
                // std::cout << "aest fail, pipeline end" << std::endl;
                // this->phase = pipeline.end()-1;
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine ae fail, can not reach given brightness, set current to best" << std::endl;
                #endif
                // ae_obj.ae_fail = false;
                best_params.lights = ae_obj.params_next.lights;
                best_params.exp_time = ae_obj.params_next.exp_time;
                best_params.exp_gain = ae_obj.params_next.exp_gain;
                *phase++; // 对于 refine phase来说，如果ae失败之后就会跳转到下一个phase，af refine都不走了，这里可实验斟酌对耗时的需求
                this->refine_ae_finish = true;
            }

            if(this->enable_af)   // 需要加这里的判断，当不打开af的时候，上面的流程会使af_obj.end_iter=true，直接进入下面的if就会phase++
            {
                if(af_obj.end_iter)
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "[AT4VS] REFINE AF Done" << std::endl;
                    #endif
                    best_params.focus_pos = af_obj.best_pos;
                    *phase++;
                }
            }

            break;
        }
        case ATPhase::AR: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AR Done\n\n");
            #endif
            *phase++; // Move to the next phase
            break;
        }
        case ATPhase::END: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] END\n\n");
            #endif
            break;
        }
        default: 
        {
            fprintf(stderr, "Invalid phase!, set phase to end\n");
            this->phase = pipeline.end()-1;
            break;
        }
    }
}
