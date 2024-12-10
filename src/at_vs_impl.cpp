/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "at_vs_impl.h"

AT4VsImpl::AT4VsImpl(const bool enable_hmap) {
    enable_hmap_ = enable_hmap;

    ae_target_brt = {64, 32, 96, 128};
    refine_code_brt = {64, 96, 32};
    refine_et_fraction = {1.0};
}

void AT4VsImpl::Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
                     const bool en_al, const bool en_af, const bool en_ae, const bool en_ar)
{
    printf("[AT4VS] Initializing: \nAL(%d), AF(%d), AE(%d), AR(%d)\nInit Params:", en_al, en_af, en_ae, en_ar);

    /* Initialize submodules configuration */
    LoadCamConf(cam_conf);

    #ifdef BUILD_WITH_LOG
        std::cout << "init load cam conf finish" << std::endl;
    #endif

    barcode_wrapper_ = &barcode_wrapper;
    #ifdef BUILD_WITH_LOG
        std::cout << "set barcode_wrapper_ done" << std::endl;
    #endif
    
    enable_al = en_al;
    enable_ae = en_ae;
    enable_af = en_af;

    pipeline.clear();
    if (en_ae | en_af) 
    {
        ae_obj.Init(ae_conf, en_al);
        pipeline.emplace_back(ATPhase::AEQT);
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
        pipeline.emplace_back(ATPhase::AF);
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
        pipeline.emplace_back(ATPhase::AEST);
        pipeline.emplace_back(ATPhase::REFINE);
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
        pipeline.emplace_back(ATPhase::AR);
        #ifdef BUILD_WITH_LOG
            std::cout << "set ar done" << std::endl;
        #endif
    }

    /* Append END to the pipeline by default */
    pipeline.emplace_back(ATPhase::END);
    phase = pipeline.begin();
    #ifdef BUILD_WITH_LOG
        std::cout << "set pipeline done" << std::endl;
    #endif

    next_params.exp_time = ae_obj.params_next.exp_time;
    next_params.exp_gain = ae_obj.params_next.exp_gain;
    next_params.lights = ae_obj.params_next.lights;
    next_params.focus_pos = af_obj.next_pos;
    best_params = next_params;

    at_roi = cv::Rect(0, 0, 0, 0);
    code_regions.clear();

    ae_target_brt_idx = 0;
    run_decode_finish = false;
    cached_image = cv::Mat();
    refine_code_brt_idx = 0;
    refine_ae_finish = false;
    af_take_onemore = false;
    af_take_onemore_finish = false;

    base_lights.clear();
    base_exp_time = 1000;
    base_exp_gain = 32;
    base_focus = 0;
    set_base = false;
    using_base = true;

    aest_statistics_num = 10;
    aest_statistics_cur = 0;
    aest_detect_success_count = 0;
    aest_decode_success_count = 0;
    aest_detect_thre = 8;

    refine_statistics_num = 10;
    refine_max_decode_rate = 0.0;
    refine_statistics_cur = 0.0;
    refine_decode_success_count = 0.0;

    refine_et_fraction_idx = 0;

    #ifdef BUILD_WITH_LOG
        printf("[AT4VS] Initialization Done\nNext Params:");
        next_params.Print();
    #endif
}

void AT4VsImpl::LoadCamConf(const CamConf &cam_conf)
{
    /* Load AF configuration */
    af_conf.start_pos = cam_conf.start_pos;
    af_conf.end_pos = cam_conf.end_pos;
    af_conf.coarse_step = cam_conf.coarse_af_step;
    af_conf.refine_step = cam_conf.refine_af_step;

    /* Load AE configuration */
    ae_conf.ae_mode = cam_conf.ae_mode;
    ae_conf.min_et_step = cam_conf.min_et_step;
    ae_conf.min_et = cam_conf.min_et;
    ae_conf.max_et = cam_conf.max_et;
    ae_conf.min_eg = cam_conf.min_eg;
    ae_conf.max_eg = cam_conf.max_eg;
    ae_conf.min_intensity = cam_conf.min_intensity;
    ae_conf.max_intensity = cam_conf.max_intensity;
    ae_conf.init_et = cam_conf.init_et;
    ae_conf.init_eg = cam_conf.init_eg;
    ae_conf.init_intensities = cam_conf.init_intensities;

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
    at_roi = roi;
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
            if(at_roi.width > 0)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "exec aeqt using setting roi " << at_roi << std::endl;
                #endif
                ae_rois.emplace_back(at_roi);
            }

            for(auto &region : ae_rois)
            {
                region.x = std::max(0, region.x);
                region.y = std::max(0, region.y);
                region.width = std::min(region.width, image.cols - region.x);
                region.height = std::min(region.height, image.rows - region.y);
            }
            #ifdef BUILD_WITH_LOG
                std::cout << "aeqt exec ae_target_brt_idx " << ae_target_brt_idx << std::endl;
            #endif
            int target_brt = ae_target_brt[ae_target_brt_idx];  // 第一次的target brightness，默认是64
            #ifdef BUILD_WITH_LOG
                    std::cout << "exec aeqt code_regions " << std::endl;
                    for(auto ele : ae_rois)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
            
            int target_thre = std::min(15, int(target_brt/2));
            ae_obj.QuickTune(image, target_brt, ae_rois, target_thre, false, 1.0);
            break;
        }
        case ATPhase::AF: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AF Running: \n");
            #endif      
            if(af_obj.end_iter && af_take_onemore)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "AF take onemore" << std::endl;
                #endif
                af_take_onemore = false;
                af_take_onemore_finish = true;
                cached_image = image;
                break;
            }

            // 当跑完第一阶段的大致对焦后，start_fit会置位，此时会跑一次解码算法，得到code_regions，之后fit阶段就会只关注code_regions的区域
            if(af_obj.start_fit && !run_decode_finish)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "af phase running decode" << std::endl;
                #endif
                // 如果没有定位到码，就不需要跑后面的refine了，会在UpdateNextParams()里更新
                auto tmp_code_regions = barcode_wrapper_->Decode(image, ar_info, at_roi);
                if (!tmp_code_regions.empty())
                {
                    code_regions = tmp_code_regions;
                }
                for(auto &region : code_regions)
                {
                    // 防止越界
                    region.x = std::max(0, region.x);
                    region.y = std::max(0, region.y);
                    region.width = std::min(region.width, image.cols - region.x);
                    region.height = std::min(region.height, image.rows - region.y);
                }

                // 如果解码函数没有定位到码，但上位机设置了roi的话，就使用上位机的roi作为refine的区域
                if(code_regions.empty() && at_roi.width > 0)
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "af coarse phase finish, no code detected, using setting roi " << at_roi << std::endl;
                    #endif
                    code_regions.emplace_back(at_roi);
                }
                
                run_decode_finish = true;
            }

            if(!af_obj.start_fit)  // 还没有进入fit阶段，说明还没有跑解码函数，此时使用sdk本身设置的roi
            {
                std::vector<cv::Rect> af_rois;
                af_rois.clear();
                if(at_roi.width > 0)
                {
                    af_rois.emplace_back(at_roi);
                }

                for(auto &region : af_rois)
                {
                    // 防止越界
                    region.x = std::max(0, region.x);
                    region.y = std::max(0, region.y);
                    region.width = std::min(region.width, image.cols - region.x);
                    region.height = std::min(region.height, image.rows - region.y);
                }

                #ifdef BUILD_WITH_LOG
                    std::cout << "af coarse phase using roi " << std::endl;
                    for(auto ele : af_rois)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                af_obj.Run(image, af_rois);  
                // af_save_count += 1;
            }
            else                   // 进入fit阶段，使用decode函数的结果
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "exec fit stage af code_regions " << std::endl;
                    for(auto ele : code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                af_obj.Run(image, code_regions);
            }

            break;
        }
        case ATPhase::AEST: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AEST Running: \n");
            #endif
            cached_image = image;
            int target_brt = ae_target_brt[ae_target_brt_idx];
            #ifdef BUILD_WITH_LOG
                std::cout << "target_brt " << target_brt << std::endl;
            #endif

            for(auto &region : code_regions)
            {
                // 防止越界
                region.x = std::max(0, region.x);
                region.y = std::max(0, region.y);
                region.width = std::min(region.width, image.cols - region.x);
                region.height = std::min(region.height, image.rows - region.y);
            }

            // 如果解码函数没有定位到码，但上位机设置了roi的话，就使用上位机的roi作为refine的区域
            if(code_regions.empty() && at_roi.width > 0)
            {
                code_regions.emplace_back(at_roi);
            }
            #ifdef BUILD_WITH_LOG
                std::cout << "exec aest code region :" << std::endl;
                for(const auto &roi : code_regions)
                {
                    std::cout << "    " << roi << std::endl;
                }
                std::cout << std::endl;
            #endif

            int target_thre = std::min(15, int(target_brt/2));
            ae_obj.QuickTune(image, target_brt, code_regions, target_thre, true, 1.0);

            break;
        }
        case ATPhase::REFINE:
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] REFINE Running: \n");
            #endif
            cached_image = image;
            #ifdef BUILD_WITH_LOG
                std::cout << "refine_ae_finish " << refine_ae_finish << std::endl;
            #endif

            for(auto &region : code_regions)
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
            if(code_regions.empty() && at_roi.width > 0)
            {
                code_regions.emplace_back(at_roi);
            }

            if(!refine_ae_finish)
            {
                int target_brt = refine_code_brt[refine_code_brt_idx];
                float fraction = refine_et_fraction[refine_et_fraction_idx];
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine ae running, target_brt: " << target_brt << ",  fraction: " << fraction << std::endl;
                    std::cout << "exec refine aeobj code region :" << std::endl;
                    for(const auto &roi : code_regions)
                    {
                        std::cout << "    " << roi << std::endl;
                    }
                    std::cout << std::endl;
                #endif

                int target_thre = std::min(10, int(target_brt/2));
                // 这里的行为是在run的过程中，就会改变灯光，不在update中再来改变
                // 不会在灯光(1,1,1,1)中把所有亮度都loop一遍，达不到某个亮度的话，直接跳到下一个亮度
                // 当(1,1,1,1)的所有都试完之后，再试(1,1,0,0)，再把所有亮度都试一遍
                // 现在的逻辑是，当去不到某个亮度的时候，会直接改变灯光再试一遍，因为只要亮度满足了，不管哪个灯光，都无所谓
                // 这样做，速度会更快些
                // 这里可能会导致某些问题，不同灯光组合，虽然全图亮度是一样的，但是亮的区域不一样
                //
                // release 5.1.1 中，修改了QuickTune()的逻辑，不会在无法达到给定亮度时调整灯光，而是会走完剩余亮度后再调整灯光
                ae_obj.QuickTune(image, target_brt, code_regions, target_thre, true, fraction);
            }
            else
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "refine af running" << std::endl;
                    std::cout << "exec refine afobj code region :" << std::endl;
                    for(const auto &roi : code_regions)
                    {
                        std::cout << roi << std::endl;
                    }
                    std::cout << std::endl;
                #endif
                #ifdef BUILD_WITH_LOG
                    std::cout << "exec refine params ";
                    next_params.Print();
                #endif
                if (enable_af)
                {
                    af_obj.Run(image, code_regions);
                }
                // af_obj.Run(image, code_regions);
            }
            break;
        }
        case ATPhase::AR: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] AR Running, run decode()");
            #endif
            auto tmp_code_regions = barcode_wrapper_->Decode(image, ar_info, at_roi);
            if(!tmp_code_regions.empty())
            {
                code_regions = tmp_code_regions;
            }
            break;
        }
        case ATPhase::END: 
        {
            #ifdef BUILD_WITH_LOG
                printf("[AT4VS] END info \n");
                std::cout << "best params: ";
                best_params.Print();
            #endif

            // 当AT结束，如果没有解到码，把最终的输出曝光设置为AEQT时的曝光值
            // if (ar_info.successful_code_type.empty())  
            if (using_base)   // 如果使用ar_info的返回值，有可能会被最后一次拍照影响，如果在refine阶段的最后一次没解到，前9次都解到，也会被判断为使用base，明显是不对的
            {
                best_params.lights = base_lights;
                best_params.exp_time = base_exp_time;
                best_params.exp_gain = base_exp_gain;
                best_params.focus_pos = base_focus;
                #ifdef BUILD_WITH_LOG
                    std::cout << "Cannot find code in the end, set to base params:" << std::endl;
                    best_params.Print();
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
                #ifdef BUILD_WITH_LOG
                    std::cout << "aeqt fail, can not reach given brightness, set current to best" << std::endl;
                    std::cout << "aeqt fial, set ae_fail = false" << std::endl;
                #endif
                ae_obj.ae_fail = false;
                ae_obj.ClearState(); //需要给后面的流程机会，aeqt用的是全图亮度，可能去不到目标亮度，但如果找到码区后，黑白比例更均匀，就有可能能达到目标亮度
                ae_obj.ResetTunningCount();
                best_params.lights = ae_obj.params_next.lights;
                best_params.exp_time = ae_obj.params_next.exp_time;
                best_params.exp_gain = ae_obj.params_next.exp_gain;
                *phase++; // Move to the next phase
            }

            base_lights = best_params.lights;
            base_exp_time = best_params.exp_time;
            base_exp_gain = best_params.exp_gain;

            break;
        }
        case ATPhase::AF: 
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "AT4VsImpl::UpdateNextParams() af phase" << std::endl;
            #endif
            next_params.focus_pos = af_obj.next_pos;
            // af完了的时候，需要跑一次解码，根据是否定位到码来决定是否还需要调整曝光
            if (af_obj.end_iter && !af_take_onemore_finish) 
            {
                #ifdef BUILD_WITH_LOG
                    printf("[AT4VS] AF Done: focus-pos=%d\n\n", af_obj.best_pos);
                #endif

                next_params.focus_pos = af_obj.best_pos;
                best_params.focus_pos = af_obj.best_pos;
                af_obj.next_pos = af_obj.best_pos;
                base_focus = best_params.focus_pos;

                af_take_onemore = true;
            }

            if (af_obj.end_iter && af_take_onemore_finish)
            {
                af_take_onemore_finish = false;
                #ifdef BUILD_WITH_LOG
                    std::cout << "UpdateNextParams phase==af, run decode()" << std::endl;
                    std::cout << "at_roi " << at_roi << std::endl;
                #endif
                auto tmp_code_regions = barcode_wrapper_->Decode(cached_image, ar_info, at_roi);
                if(!tmp_code_regions.empty())
                {
                    code_regions  = tmp_code_regions;
                }
                #ifdef BUILD_WITH_LOG
                    std::cout << "code_regions.size " << code_regions.size() << std::endl; 
                    for(auto ele : code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif
                
                // if(ar_info.successful_code_type.empty())//如果没解到码，就看有没有打开ae，打开了的话，就跳转到AEST
                if(code_regions.empty())//如果没解到码，就看有没有打开ae，打开了的话，就跳转到AEST
                {
                    ae_obj.end_qt = false;
                    // ae_target_brt_idx = (ae_target_brt_idx + 1) % ae_target_brt.size();
                    auto it = std::find(pipeline.begin(), pipeline.end(), ATPhase::AEST);
                    if(it != pipeline.end())   // 打开了ae，则跳到aest
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "af finish and no code detected, phase = aest" << std::endl;
                        #endif
                        auto phase_idx = std::distance(pipeline.begin(), it);
                        phase = pipeline.begin() + phase_idx;
                        code_regions.clear();   // 跳转前先清空 code_regions
                    }
                    else     // 没开ae，跳到下一个，ar 或 end
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "af finish and no code detected, phase++ " << std::endl;
                        #endif
                        *phase++; // Move to the next phase
                        code_regions.clear();  // // 跳转前先清空 code_regions
                    }
                }
                else    // 如果解了码的话，开了ae就跳过aest,由于aest默认有refine，所有下面是跳过refine，没开ae就自动跳到下一个
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "af finish and code detected, phase++" << std::endl;
                    #endif
                    *phase++; // Move to the next phase
                    code_regions.clear();   // // 跳转前先清空 code_regions
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

            if(ae_obj.end_qt)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "aest ae_obj.end_qt=true, run decode()" << std::endl;
                #endif
                auto tmp_code_regions = barcode_wrapper_->Decode(cached_image, ar_info, at_roi);

                #ifdef BUILD_WITH_LOG
                    std::cout << "aest target brt " << ae_target_brt[ae_target_brt_idx] << ", code regions size " << tmp_code_regions.size() << std::endl;
                #endif
                // if(!tmp_code_regions.empty())
                // {
                //     code_regions = tmp_code_regions;
                // }
                #ifdef BUILD_WITH_LOG
                    std::cout << "code_regions " << std::endl;
                    for(auto ele : tmp_code_regions)
                    {
                        std::cout << ele << std::endl;
                    }
                #endif

                // 记录一组不太差的亮度参数，用于在无码场景时的最终结果
                // 这里选择的是aest时的第一个调整亮度，调整灯光后的不需要
                if(ae_target_brt_idx == 0 && !set_base)
                {
                    set_base = true;
                    base_lights = ae_obj.params_next.lights;
                    base_exp_time = ae_obj.params_next.exp_time;
                    base_exp_gain = ae_obj.params_next.exp_gain;
#ifdef BUILD_WITH_LOG
                        std::cout << "using aset target brt=" << ae_target_brt[0] << " as base param" << std::endl;
                    #endif
                }

                if (aest_statistics_cur == 0)
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "reset aest_regions_canvas" << std::endl;
                    #endif
                    aest_regions_canvas = cv::Mat(cached_image.size(), CV_8UC1, cv::Scalar(0));
                }

                if (aest_statistics_cur < aest_statistics_num)
                {
                    if(!tmp_code_regions.empty())
                    {
                        aest_detect_success_count += 1;

                        cv::Mat tmp_canvas(aest_regions_canvas.size(), CV_8UC1, cv::Scalar(0));
                        for(auto r : tmp_code_regions)
                        {
                            cv::rectangle(tmp_canvas, r, {1}, -1);
                        }
                        aest_regions_canvas += tmp_canvas;
                    }
                    
                    if(!ar_info.successful_code_type.empty())
                    {
                        aest_decode_success_count += 1;
                    }
                    #ifdef BUILD_WITH_LOG
                        // std::cout << "aest_statistics_cur: " << aest_statistics_cur << ", aest_detect_success_count: " << aest_detect_success_count << std::endl;
                        std::cout << "aest_statistics_cur: " << aest_statistics_cur << ", aest_decode_success_count: " << aest_decode_success_count << std::endl;
                    #endif
                    aest_statistics_cur += 1;
                }
                else
                {
                    double min_val, max_val;
                    cv::minMaxLoc(aest_regions_canvas, &min_val, &max_val);
                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest_statistics_cur == aest_statistics_num, max_val=" << max_val << std::endl;
                    #endif
                    // aest时也会记录是否能解码，以解码作为最高优先级，避免出现某些灯光是能找到码但是不能解码的情况
                    if(aest_decode_success_count > 0)
                    {
                        next_params.exp_gain = ae_obj.params_best.exp_gain;
                        next_params.exp_time = ae_obj.params_best.exp_time;
                        next_params.lights = ae_obj.params_best.lights;

                        best_params.lights = ae_obj.params_best.lights;
                        best_params.exp_time = ae_obj.params_best.exp_time;
                        best_params.exp_gain = ae_obj.params_best.exp_gain;
                        #ifdef BUILD_WITH_LOG
                            // std::cout << "max_val > detect_thre, move to next phase" << std::endl;
                            std::cout << "aset decode success, move to next pahse" << std::endl;
                        #endif
                        *phase++;
                    }
                    else
                    {
                        // 解不到码的时候，就是用找码率最高的那个
                        if(aest_params_map.find(int(max_val)) == aest_params_map.end())
                        {
                            CamParams tmp_params;
                            tmp_params.exp_gain = ae_obj.params_best.exp_gain;
                            tmp_params.exp_time = ae_obj.params_best.exp_time;
                            tmp_params.lights = ae_obj.params_best.lights;
                            tmp_params.focus_pos = best_params.focus_pos;
                            aest_params_map[max_val] = tmp_params;

                            #ifdef BUILD_WITH_LOG
                                std::cout << "aest params add ele at success_count: " << int(max_val) << std::endl;
                                tmp_params.Print();
                            #endif
                        }
                        else
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "find exist aest_params_map key " << int(max_val) << std::endl;
                            #endif
                        }

                        aest_statistics_cur = 0;
                        aest_detect_success_count = 0;
                        ae_target_brt_idx += 1;
                        if(ae_target_brt_idx == ae_target_brt.size()) // 当前亮度都轮询完之后，更换灯光后重新轮询亮度
                        {
                            ae_target_brt_idx = 0;
                            bool update_success = ae_obj.UpdateLights();
                            #ifdef BUILD_WITH_LOG
                                std::cout << "AEST aest_statistics_cur can not greater than thres, updatelights() " << update_success << std::endl;
                            #endif
                            next_params.lights = ae_obj.params_next.lights;
                            next_params.exp_gain = ae_obj.params_next.exp_gain;
                        }
                    }
                }
            }
            else
            {
                if(ae_obj.exceed_tunning_count)
                {
                    ae_obj.end_qt = false;
                    ae_target_brt_idx += 1;  // 查询下一个亮度
                    ae_obj.ResetTunningCount();
                    if(ae_target_brt_idx == ae_target_brt.size()) // 当前亮度都轮询完之后，更换灯光后重新轮询亮度
                    {
#ifdef BUILD_WITH_LOG
                        std::cout << "AT4VsImpl::UpdateNextParams() AEST ae_obj exceed tunning count, updatelights()" << std::endl;
#endif
                        ae_target_brt_idx = 0;
                        ae_obj.UpdateLights();   // 如果一直都没有解出码，就会导致ae_obj.ae_fail，就会跳出ae
                        next_params.lights = ae_obj.params_next.lights;
                        next_params.exp_gain = ae_obj.params_next.exp_gain;

                    }
#ifdef BUILD_WITH_LOG
                    std::cout << "ae_obj exceed tunning count, next brt " << ae_target_brt[ae_target_brt_idx] << std::endl;
#endif
                }
                else
                {
                    break;
                }
            }

            if (ae_obj.ae_fail)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae_obj.ae_fail, means update lights fail" << std::endl;
                #endif
                ae_obj.ClearState();
                ae_obj.ResetTunningCount();

                if (!aest_params_map.empty())
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "all ele in aest_params_map" << std::endl;
                        for(auto ele : aest_params_map)
                        {
                            std::cout << ele.first << ", ";
                            ele.second.Print();
                        }
                    #endif
                    
                    CamParams aest_best_params = aest_params_map.rbegin()->second;
                    next_params.exp_gain = aest_best_params.exp_gain;
                    next_params.exp_time = aest_best_params.exp_time;
                    next_params.lights = aest_best_params.lights;

                    best_params.exp_gain = aest_best_params.exp_gain;
                    best_params.exp_time = aest_best_params.exp_time;
                    best_params.lights = aest_best_params.lights;
                    #ifdef BUILD_WITH_LOG
                        std::cout << "select aest best param" << std::endl;
                        aest_best_params.Print();
                    #endif
                    ae_obj.SetParam(aest_best_params.exp_time, aest_best_params.exp_gain, aest_best_params.lights);
                }
                else
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest_params_map.empty()==true, using next param to current" << std::endl;
                    #endif
                }

                // 记录一组不太差的亮度参数，用于在无码场景时的最终结果
                // 这里用的是aest失败时，最接近时的值
                if(!set_base)
                {
                    base_lights = ae_obj.params_next.lights;
                    base_exp_time = ae_obj.params_next.exp_time;
                    base_exp_gain = ae_obj.params_next.exp_gain;
                    set_base = true;
                    #ifdef BUILD_WITH_LOG
                        std::cout << "aest fail, using last try as base param" << std::endl;
                    #endif
                }

                *phase++;
                break;
            }

            break;
        }
        case ATPhase::REFINE: 
        {
            // next_params.lights = ae_obj.params_next.lights;
            // next_params.exp_time = ae_obj.params_next.exp_time;
            // next_params.exp_gain = ae_obj.params_next.exp_gain;
            // next_params.focus_pos = af_obj.next_pos;

            #ifdef BUILD_WITH_LOG
                    std::cout << "AT4VsImpl::UpdateNextParams() refine phase" << std::endl;
            #endif
            // refine 阶段不会再调整灯光，只会调整曝光
            if(!refine_ae_finish)
            {
                next_params.lights = ae_obj.params_next.lights;
                next_params.exp_time = ae_obj.params_next.exp_time;
                next_params.exp_gain = ae_obj.params_next.exp_gain;

                if(ae_obj.end_qt)
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "refine phase: ae finish, run decode()" << std::endl;
                    #endif
                    ae_obj.end_qt = false;
                    auto tmp_code_regions = barcode_wrapper_->Decode(cached_image, ar_info, at_roi);
                    #ifdef BUILD_WITH_LOG
                        std::cout << "refine ae target brt " << refine_code_brt[refine_code_brt_idx] << ", code regions size " << tmp_code_regions.size() << std::endl;
                    #endif
                    if(!ar_info.successful_code_type.empty())   // 在refine阶段，要解到了码，才能给定码区，因为tmp_code_regions有可能是错的，不能被一个有可能是错的region引导偏
                    {
                        code_regions = tmp_code_regions;
                        using_base = false;      // 只要有一次解到码，就不会用base
                    }
                    #ifdef BUILD_WITH_LOG
                        std::cout << "check all brt" << std::endl;
                    #endif

                    if(refine_statistics_cur < refine_statistics_num)
                    {
                        if(!ar_info.successful_code_type.empty())
                        {   
                            #ifdef BUILD_WITH_LOG
                                std::cout << "brt " << refine_code_brt[refine_code_brt_idx] << " decode success" << std::endl;
                            #endif
                            refine_decode_success_count += 1;
                        }
                        else
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "brt " << refine_code_brt[refine_code_brt_idx] << " decode fail" << std::endl;
                            #endif
                        }
                        refine_statistics_cur += 1;
                        #ifdef BUILD_WITH_LOG
                            std::cout << "refine_statistics_cur " << refine_statistics_cur << std::endl;
                            std::cout << "refine_decode_success_count " << refine_decode_success_count << std::endl;
                        #endif
                    }
                    else
                    {
                        float decode_rate = refine_decode_success_count / refine_statistics_num;
                        #ifdef BUILD_WITH_LOG
                            std::cout << "finish one loop, decode_rate " << decode_rate << std::endl;
                            std::cout << "refine_max_decode_rate: " << refine_max_decode_rate << std::endl;
                        #endif
                        if (decode_rate > refine_max_decode_rate)
                        {
                            refine_max_decode_rate = decode_rate;
                            best_params.lights = ae_obj.params_best.lights;
                            best_params.exp_time = ae_obj.params_best.exp_time;
                            best_params.exp_gain = ae_obj.params_best.exp_gain;
                            #ifdef BUILD_WITH_LOG
                                std::cout << "decode rate update, max is " << decode_rate << std::endl;
                                std::cout << "    exp_time=" << best_params.exp_time << ", exp_gain=" << best_params.exp_gain << std::endl;
                            #endif
                        } 
                        refine_statistics_cur = 0;
                        refine_decode_success_count = 0;
                        refine_code_brt_idx += 1;
                        #ifdef BUILD_WITH_LOG
                            if (refine_code_brt_idx < refine_code_brt.size())
                            {
                                std::cout << "update loop data, next brt is " << refine_code_brt[refine_code_brt_idx] << std::endl;
                            }
                        #endif

                        if (refine_code_brt_idx == refine_code_brt.size())
                        {
                            refine_code_brt_idx = 0;
                            if (refine_et_fraction_idx < refine_et_fraction.size())
                            {
                                refine_et_fraction_idx += 1;
                            }
                        }

                        // // 当解码率足够高的时候，就不跑后面的亮度了
                        // if(refine_max_decode_rate > 0.95)
                        // {
                        //     refine_code_brt_idx = refine_code_brt.size();
                        //     #ifdef BUILD_WITH_LOG
                        //         std::cout << "refine decode rate high enough, stop loop brt" << std::endl;
                        //     #endif
                        // }
                    }
                    // if(refine_code_brt_idx >= refine_code_brt.size())
                    if(refine_et_fraction_idx==refine_et_fraction.size() || refine_max_decode_rate > 0.85)
                    {
                        #ifdef BUILD_WITH_LOG
                            if(refine_et_fraction_idx==refine_et_fraction.size())
                            {
                                std::cout << "loop all et eg combination, finish refine ae" << std::endl;
                            }
                            if(refine_max_decode_rate > 0.85)
                            {
                                std::cout << "refine decode rate high enough, stop loop brt, finish refine ae" << std::endl;
                            }
                        #endif

                        #ifdef BUILD_WITH_LOG
                            std::cout << "refine loop all brt step, refine_ae_finish=true" << std::endl;
                        #endif
                        refine_ae_finish = true;
                        if (enable_af)           // 要打开了af才能进入 refine af
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "refine run af reset samples, enter 1" << std::endl;
                            #endif
                            af_obj.ResetSamples();
                        }
                        else 
                        {
                            *phase++;    // 如果没有打开af，如vs600，那么refine ae后直接就到下一阶段，跳过refine af
                        }
                        next_params.exp_time = best_params.exp_time;
                        next_params.exp_gain = best_params.exp_gain;
                        next_params.focus_pos = af_obj.next_pos;
                        #ifdef BUILD_WITH_LOG
                            std::cout << "next_params ";
                            next_params.Print();
                        #endif
                    }
                }
                else
                {
                    if(!ae_obj.exceed_tunning_count)
                    {
                        break;
                    }
                    else
                    {
                        refine_code_brt_idx += 1;
                        if (refine_code_brt_idx == refine_code_brt.size())
                        {
                            refine_code_brt_idx = 0;
                            if (refine_et_fraction_idx < refine_et_fraction.size())
                            {
                                refine_et_fraction_idx += 1;
                            }
                        }
                        ae_obj.ResetTunningCount();
                        #ifdef BUILD_WITH_LOG
                            if (refine_code_brt_idx < refine_code_brt.size())
                            {
                                std::cout << "refine exceed tunning count, next brt is " << refine_code_brt[refine_code_brt_idx] << std::endl;
                            }
                        #endif
                    }

                    // if(refine_code_brt_idx >= refine_code_brt.size())
                    if(refine_et_fraction_idx==refine_et_fraction.size())
                    {
                        #ifdef BUILD_WITH_LOG
                            std::cout << "refine ae can not reach given brightness and refine_code_brt_idx exceed size, refine_ae_finish=true" << std::endl;
                        #endif
                        refine_ae_finish = true;
                        if (enable_af)           // 要打开了af才能进入 refine af
                        {
                            #ifdef BUILD_WITH_LOG
                                std::cout << "refine run af reset samples, enter 2" << std::endl;
                            #endif
                            af_obj.ResetSamples();
                        }
                        else 
                        {
                            *phase++;    // 如果没有打开af，如vs600，那么refine ae后直接就到下一阶段，跳过refine af
                        }
                        next_params.exp_time = best_params.exp_time;
                        next_params.exp_gain = best_params.exp_gain;
                        next_params.focus_pos = af_obj.next_pos;
                        ae_obj.ae_fail = true;
                    }
                }
            }

            if(refine_ae_finish && enable_af)   // 需要加这里的判断，当不打开af的时候，上面的流程会使af_obj.end_iter=true，直接进入下面的if就会phase++
            {
                next_params.focus_pos = af_obj.next_pos;
                if(af_obj.end_iter)
                {
                    #ifdef BUILD_WITH_LOG
                        std::cout << "[AT4VS] REFINE AF Done, update best focus = " << af_obj.best_pos << std::endl;
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
            phase = pipeline.end()-1;
            break;
        }
    }
}
