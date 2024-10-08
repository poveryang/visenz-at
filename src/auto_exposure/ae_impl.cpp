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
        if (n_lights > 1)
        {
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
            // std::vector<int> close_all(n_lights, 0);
            // lights_sets = {close_all, polarized, unpolarized, all};
            this->lights_sets = {polarized, unpolarized, all};
        }
        else
        {
            all.emplace_back(this->max_intensity);
            this->lights_sets = {all};
        }
    } 
    else 
    {
        this->lights_sets = {ae_conf.init_intensities};
    }
    params_next.lights = this->lights_sets.back();
    this->lights_sets.pop_back();

    // 每次切换灯光，都会重新计数tuning_step，当超过max_tuning_step后，就是切换到下一组灯光
    this->tuning_count = 0;
    this->max_tuning_count = 15;
    this->ae_fail = false;    
}

bool AEImpl::QuickTune(const cv::Mat &image, int brt_target, const std::vector<cv::Rect> &rois, int brt_diff_thre, bool enable_switch) 
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
            if (enable_switch)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae enable_switch=true" << std::endl;
                #endif
                if(this->lights_sets.empty())
                {
                    // 在quick tune阶段，当试完所有灯光组合，仍无法达到brt target后，宣布ae失败，等待上层处理
                    #ifdef BUILD_WITH_LOG
                        std::cout << "set ae fail" << std::endl;
                    #endif
                    this->tuning_count = 0;
                    this->ae_fail = true; 
                    return false;
                }
                
                this->tuning_count = 0;
                params_next.lights = this->lights_sets.back();
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae impl change lights " << params_next.lights[0] << ", " << params_next.lights[1] << ", " << params_next.lights[2] << ", " << params_next.lights[3] << std::endl;
                #endif
                this->lights_sets.pop_back();
            }
            else
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae enable_switch=false, set ae fail, but not change lights" << std::endl;
                #endif
                this->tuning_count = 0;
                this->ae_fail = true; 
                return false;
            }
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
    params_next.lights = this->lights_sets.back();
    params_next.exp_gain = this->init_eg;
    this->lights_sets.pop_back();
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
        // mean_brt = cv::mean(image)[0];

        std::vector<cv::Rect> sampling_rois;
        const std::array<int, 2> k_grid_size = {8, 8}; // image会被分割成grid_size份 {rows, cols}
        const float k_ratio = 0.33;     // 每个grid_size只会取中间部分统计
        const int k_row_step = image.rows / k_grid_size[0];
        const int k_col_step = image.cols / k_grid_size[1];
        // cv::Mat canvas;
        // cv::cvtColor(image, canvas, cv::COLOR_GRAY2BGR);
        for(int row=0; row<k_grid_size[0]; row++)
        {
            for(int col=0; col<k_grid_size[1]; col++)
            {
                cv::Rect roi;
                roi.x = int(k_col_step * col + k_col_step * (1-k_ratio) / 2);
                roi.y = int(k_row_step * row + k_row_step * (1-k_ratio) / 2);
                roi.width = int(k_col_step * k_ratio);
                roi.height = int(k_row_step * k_ratio);

                sampling_rois.emplace_back(roi);
                // cv::rectangle(canvas, roi, {0,255,0}, 1);
            }
        }
        for(const auto &roi : sampling_rois)
        {
            cv::Mat roi_img = image(roi);
            mean_brt += cv::mean(roi_img)[0];
        }
        mean_brt /= sampling_rois.size();
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
        // 这个if是在只有cur_eg > init_eg 的时候，才会调整eg
        if(cur_eg > this->init_eg)
        {
            eg_scale = this->init_eg * 1.0f / cur_eg;
        }
        et_scale = std::min(total_scale/eg_scale, 1.0f);

        if (et_scale < 1 && cur_exposure < this->min_et+1)
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "exposure time down to limit, scale eg at this situation" << std::endl;
            #endif
            eg_scale = total_scale;
            et_scale = 1.0f;
        }
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
        if(cur_eg > 80)   // 实验表明，亮度曲线是分段的，统一设置当超过80后，限制放大系数，以免超调
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
    if(next_eg - cur_eg > 30)   // 限制增益的增长，以免超调
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