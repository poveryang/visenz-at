#include "ae_impl.h"
#include <string>

using namespace ae;

AEImpl::AEImpl(const AEConf &ae_conf, bool en_al) 
{
    /* Camera config related to AE */
    this->mode = ae_conf.ae_mode;
    this->min_et = ae_conf.min_et;
    this->max_et = std::min(ae_conf.max_et, this->MAX_ET_LIMIT);
    this->min_eg = ae_conf.min_eg;
    this->max_eg = ae_conf.max_eg;
    this->min_intensity = ae_conf.min_intensity;
    this->max_intensity = ae_conf.max_intensity;

    this->init_eg = ae_conf.init_eg;

    /* Basic variables */
    params_next.exp_time = ae_conf.init_et;
    params_next.exp_gain = this->init_eg;
    if (en_al) 
    {
        std::vector<int> all;
        std::vector<int> polarized;
        std::vector<int> unpolarized;
        size_t n_lights = ae_conf.init_intensities.size();
        for (size_t i = 0; i < n_lights; i++) 
        {
            all.emplace_back(this->max_intensity);
            if (i < n_lights / 2) 
            {
                polarized.emplace_back(0);
                unpolarized.emplace_back(this->max_intensity);
            } 
            else 
            {
                polarized.emplace_back(this->max_intensity);
                unpolarized.emplace_back(0);
            }
        }
        std::vector<int> close_all(n_lights, 0);
        lights_sets = {close_all, polarized, unpolarized, all};
    } 
    else 
    {
        lights_sets = {ae_conf.init_intensities};
    }
    params_next.lights = lights_sets.back();
    this->lights_sets.pop_back();

    // 每次切换灯光，都会重新计数tuning_step，当超过max_tuning_step后，就是切换到下一组灯光
    this->tuning_count = 0;
    this->max_tuning_count = 15;
    this->ae_fail = false;    
}

bool AEImpl::QuickTune(const cv::Mat &image, int brt_target, const std::vector<cv::Rect> &rois, int brt_diff_thre) 
{
    /* Calc metrics in current frame */
    #ifdef BUILD_WITH_LOG
        std::cout << "ae brt_target " << brt_target << ", brt_diff_thre" << brt_diff_thre << std::endl;
    #endif
    CalcMetrics(image, rois);
    this->tuning_count += 1;
    #ifdef BUILD_WITH_LOG
        std::cout << "ae tuning count " << this->tuning_count << std::endl;
    #endif

    if (abs(status_cur.brt - brt_target) < brt_diff_thre) 
    {
        params_best = params_next;
        this->tuning_count = 0;
        return true;
    } 
    else 
    {
        if(this->tuning_count > this->max_tuning_count)
        {
            if(this->lights_sets.empty())
            {
                // 在quick tune阶段，当试完所有灯光组合，仍无法达到brt target后，宣布ae失败，等待上层处理
                #ifdef BUILD_WITH_LOG
                    std::cout << "set ae fail" << std::endl;
                #endif
                // this->tuning_count = 0; // 这里不一定需要清零，程序的运行有可能是以下逻辑：
                                           // 1、在 AEST 时试遍了所有灯光，都没达到需要的亮度，此时 this->tuning_count > this->max_tuning_count
                                           // 2、状态会跳转到REFINE
                                           // 3、REFINE的第一步是调节ae，就会再进一次这个函数，那么也不会再调了，因为怎么也调不到了
                this->ae_fail = true; 
                return false;
            }
             
            this->tuning_count = 0;
            params_next.lights = lights_sets.back();
            #ifdef BUILD_WITH_LOG
                std::cout << "ae impl change lights " << params_next.lights[0] << ", " << params_next.lights[1] << ", " << params_next.lights[2] << ", " << params_next.lights[3] << std::endl;
            #endif
            lights_sets.pop_back();
        }

        this->UpdateExposureAndGain(brt_target);

        return false;
    }
}

bool AEImpl::UpdateLights()
{
    this->tuning_count = 0;
    if(this->lights_sets.empty())
    {
        #ifdef BUILD_WITH_LOG
            std::cout << "set ae fail, update lights empty" << std::endl;
        #endif
        this->ae_fail = true; 
        return false;
    }
    params_next.lights = lights_sets.back();
    params_next.exp_gain = this->init_eg;
    lights_sets.pop_back();
    return true;
}

void AEImpl::CalcMetrics(const cv::Mat &image, const std::vector<cv::Rect> &rois) 
{
    /* Calc metrics */
    double brt =-1;
    brt = CalcMeanBrt(image, rois);

    /* Update status */
    status_cur.params = params_next;
    status_cur.brt = brt;

    #ifdef BUILD_WITH_LOG
        status_cur.Print();  // print status
    #endif
}

double AEImpl::CalcMeanBrt(const cv::Mat &image, const std::vector<cv::Rect> &rois) 
{
    /* Calc image mean brightness */
    double mean_brt = 0;
    if(rois.empty())
    {
        mean_brt = cv::mean(image)[0];
    }
    else
    {
        for(const auto &roi : rois)
        {
            cv::Mat roi_img = image(roi);
            mean_brt += cv::mean(roi_img)[0];
        }
        mean_brt /= rois.size();
    }

    return mean_brt;
}

void AEImpl::UpdateExposureAndGain(int brt_target)
{
    #ifdef BUILD_WITH_LOG
        std::cout << "UpdateExposureAndGain()" << std::endl;
    #endif
    float brt_cur = static_cast<float>(this->status_cur.brt);
    int cur_exposure = this->status_cur.params.exp_time;
    int cur_eg = this->status_cur.params.exp_gain;

    float total_scale = brt_target * 1.0 / brt_cur;

    float eg_scale = 1.0f;
    float et_scale = 1.0f;
    // 当需要降低亮度的时候，由于曝光为0时，是能基本保证亮度为0，因此会限定gain的下限为32，其余分量全压在曝光上
    if(total_scale < 1)
    {   
        #ifdef BUILD_WITH_LOG
            std::cout << "decrease brt" << std::endl;
        #endif
        // 降亮度的时候，如果gain<init，则不调整eg了
        if(cur_eg > this->init_eg)
        {
            eg_scale = this->init_eg * 1.0f / cur_eg;
        }
        et_scale = std::min(total_scale/eg_scale, 1.0f);
    }
    else
    {
        #ifdef BUILD_WITH_LOG
            std::cout << "increase brt" << std::endl;
        #endif
        et_scale = total_scale;
        if(cur_exposure * total_scale > this->max_et)
        {
            et_scale = this->max_et * 1.0f / cur_exposure;
            eg_scale = std::min(2.0f, total_scale / et_scale);
        }
        if(cur_eg > 80)
        {
            eg_scale = std::min(1.2f, eg_scale);
        }
    }

    #ifdef BUILD_WITH_LOG
        std::cout << "et scale " << et_scale << ",  eg scale" << eg_scale << std::endl; 
    #endif

    int next_et = std::min(int(cur_exposure*et_scale), this->max_et);
    next_et = std::max(next_et, this->min_et);
    int next_eg = std::min(int(cur_eg*eg_scale), this->max_eg);
    next_eg = std::max(next_eg, this->min_eg);
    if(next_eg - cur_eg > 30)
    {
        #ifdef BUILD_WITH_LOG
            std::cout << "limit eg increase in 30" << std::endl;
        #endif
        next_eg = cur_eg + 30;
    }

    #ifdef BUILD_WITH_LOG
        std::cout << "next et " << next_et << ",  next eg " << next_eg << std::endl;
    #endif

    this->params_next.exp_time = next_et;
    this->params_next.exp_gain = next_eg;
}