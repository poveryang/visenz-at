#include "ae_impl.h"
#include <string>
#include <numeric>
#include <algorithm>

using namespace ae;

int ae_save_count = 0;

AEImpl::AEImpl(const AEConf &ae_conf, bool en_al) 
{
    /* Camera config related to AE */
    mode = ae_conf.ae_mode;
    min_et_step = ae_conf.min_et_step;
    min_et = ae_conf.min_et;
    max_et = std::min(ae_conf.max_et, MAX_ET_LIMIT);
    min_eg = ae_conf.min_eg;
    max_eg = ae_conf.max_eg;
    min_intensity = ae_conf.min_intensity;
    max_intensity = ae_conf.max_intensity;

    init_et = ae_conf.init_et;
    init_eg = ae_conf.init_eg;

    /* Basic variables */
    params_next.exp_time = (min_et_step != 1) ? std::max(min_et, min_et_step) : init_et;
    params_next.exp_gain = init_eg;
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
                all.emplace_back(max_intensity);
                if (i < n_lights / 2) 
                {
                    polarized.emplace_back(0);
                    unpolarized.emplace_back(max_intensity);
                } 
                else 
                {
                    polarized.emplace_back(max_intensity);
                    unpolarized.emplace_back(0);
                }
            }
            // std::vector<int> close_all(n_lights, 0);
            // lights_sets = {close_all, polarized, unpolarized, all};
            lights_sets = {unpolarized, polarized, all};
        }
        else
        {
            all.emplace_back(max_intensity);
            lights_sets = {all};
        }
    } 
    else 
    {
        lights_sets = {ae_conf.init_intensities};
    }
    params_next.lights = lights_sets.back();
    lights_sets.pop_back();

    // 每次切换灯光，都会重新计数tuning_step，当超过max_tuning_step后，就是切换到下一组灯光
    tuning_count = 0;
    max_tuning_count = 15;
    ae_fail = false; 
    exceed_tunning_count = false;   

    /* Initialize adaptive weight strategy */
    et_weight = 0.7;                    // initial weight for exposure time
    eg_weight = 0.3;                    // initial weight for exposure gain
    marginal_et_efficiency = 1.0;       // initial marginal efficiency
    marginal_eg_efficiency = 1.0;       // initial marginal efficiency
    history_window_size = 5;        // window size for efficiency calculation
    et_history.clear();
    eg_history.clear();
    brt_history.clear();
}

bool AEImpl::QuickTune(const cv::Mat &image, int brt_target, const std::vector<cv::Rect> &rois, int brt_diff_thre, bool enable_switch)
{
    /* Calc metrics in current frame */
    #ifdef BUILD_WITH_LOG
        std::cout << "ae brt_target " << brt_target << ", brt_diff_thre" << brt_diff_thre << std::endl;
    #endif
    CalcMetrics(image, rois, brt_target);
    tuning_count += 1;
    #ifdef BUILD_WITH_LOG
        std::cout << "ae tuning count " << tuning_count << std::endl;
    #endif

    if (abs(status_cur.brt - brt_target) < brt_diff_thre) 
    {
        params_best = params_next;
        #ifdef BUILD_WITH_LOG
            std::cout << "reach giving brt, reset tuning_count = 0" << std::endl;
        #endif
        tuning_count = 0;
        return true;
    } 
    else 
    {
        if(tuning_count > max_tuning_count)
        {
            exceed_tunning_count = true;
            tuning_count = 0;
            if(!enable_switch)
            {
                ae_fail = true;
            }
            return false;

            /*
            if (enable_switch)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae enable_switch=true" << std::endl;
                #endif
                if(lights_sets.empty())
                {
                    // 在quick tune阶段，当试完所有灯光组合，仍无法达到brt target后，宣布ae失败，等待上层处理
                    #ifdef BUILD_WITH_LOG
                        std::cout << "set ae fail" << std::endl;
                    #endif
                    tuning_count = 0;
                    ae_fail = true; 
                    return false;
                }
                
                tuning_count = 0;
                params_next.lights = lights_sets.back();
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae impl change lights " << params_next.lights[0] << ", " << params_next.lights[1] << ", " << params_next.lights[2] << ", " << params_next.lights[3] << std::endl;
                #endif
                lights_sets.pop_back();
            }
            else
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "ae enable_switch=false, set ae fail, but not change lights" << std::endl;
                #endif
                tuning_count = 0;
                ae_fail = true; 
                return false;
            }
            */
        }

        this->UpdateExposureAndGain(brt_target);

        return false;
    }
}

bool AEImpl::UpdateLights()
{
    tuning_count = 0;
    exceed_tunning_count = false;
    if(lights_sets.empty())
    {
        #ifdef BUILD_WITH_LOG
            std::cout << "set ae fail, update lights empty" << std::endl;
        #endif
        ae_fail = true; 
        return false;
    }
    params_next.lights = lights_sets.back();
    params_next.exp_gain = init_eg;
    lights_sets.pop_back();
    
    // Reset history when switching lights
    ResetHistory();
    
    return true;
}

void AEImpl::CalcMetrics(const cv::Mat &image, const std::vector<cv::Rect> &rois, int brt_target) 
{
    /* Calc metrics */
    const double brt = CalcMeanBrt(image, rois, brt_target);

    /* Update status */
    status_cur.params = params_next;
    status_cur.brt = brt;

    #ifdef BUILD_WITH_LOG
        status_cur.Print();  // print status
    #endif
}

double AEImpl::CalcMeanBrt(const cv::Mat &image, const std::vector<cv::Rect> &rois, int brt_target) 
{
    /* Calc image mean brightness */
    double mean_brt = 0;
    if(rois.empty())
    {
        // mean_brt = cv::mean(image)[0];

        std::vector<cv::Rect> sampling_rois;
        const std::array<int, 2> k_grid_size = {8, 8}; // image会被分割成grid_size份 {rows, cols}
        const float k_ratio = 1;     // 每个grid_size只会取中间部分统计
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

                if (roi.br().x > image.cols-1)
                {
                    roi.x = image.cols - roi.width - 1;
                }
                if (roi.br().y > image.rows-1)
                {
                    roi.y = image.rows - roi.height - 1;
                }

                sampling_rois.emplace_back(roi);
                // cv::rectangle(canvas, roi, {0,255,0}, 1);
            }
        }
        
        std::vector<double> brt_vec;
        for (const auto &roi : sampling_rois)
        {
            cv::Mat roi_img = image(roi);
            auto mean_val = cv::mean(roi_img)[0];
            brt_vec.emplace_back(mean_val);
        }
        double brt_max = *std::max_element(brt_vec.begin(), brt_vec.end());
        double brt_avg = std::accumulate(brt_vec.begin(), brt_vec.end(), 0.0);
        brt_avg /= brt_vec.size();

        double gama = 1 + 0.5 * (1 - brt_avg/brt_max);
        std::vector<double> weights;
        for(auto &li : brt_vec)
        {

            double w;
            if (li <= brt_avg){
                w = std::pow(0.6, gama);
                // w = std::pow(li/brt_avg, gama);
            } else {
                w = std::pow(li/brt_avg, gama);
                w = std::min(2.0, w);
            }
            weights.emplace_back(w);
        }
        
        std::vector<double> ori_weights(weights.begin(), weights.end());
        double weights_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        for(auto &wi : weights)
        {
            wi = wi / weights_sum;
        }

        for(int i=0; i<brt_vec.size(); i++)
        {
            mean_brt += weights[i] * brt_vec[i];
        }

        // for(int r=0; r<k_grid_size[0]; r++)
        // {
        //     for(int c=0; c<k_grid_size[1]; c++)
        //     {
        //         int index = r * k_grid_size[0] + c;
        //         auto roi = sampling_rois[index];
        //         auto ori_mean = brt_vec[index];
        //         auto weighted_mean = weights[index] * brt_vec[index] * k_grid_size[0] * k_grid_size[1];
        //         auto ori_wi = ori_weights[index];

        //         cv::rectangle(canvas, roi, {0,255,0}, 1);
        //         cv::putText(canvas, std::to_string(int(ori_mean)), {roi.x, roi.y+roi.height}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0,255,0});
        //         cv::putText(canvas, std::to_string(int(weighted_mean)), {roi.x, roi.y+roi.height-15}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {255,0,0});

        //         std::string ori_wi_str = std::to_string(ori_wi);
        //         cv::putText(canvas, ori_wi_str.substr(0, 5), {roi.x, roi.y+roi.height-30}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0,0,255});

        //     }
        // }

        // cv::imwrite("/usr/scanner/debug/at/test_img/ae/id_"+std::to_string(ae_save_count)+"_tar_"+ std::to_string(brt_target) + "_weightMean_"+std::to_string(int(mean_brt)) + "_oriMean_"+ std::to_string(int(brt_avg)) + ".png", canvas);
        // ae_save_count += 1;
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
    
    // Calculate marginal efficiency and update weights
    CalculateMarginalEfficiency(brt_target);
    UpdateAdaptiveWeights();
    
    const double brt_cur = std::max(status_cur.brt, 1.0);  // Prevent division by zero
    const int cur_et = status_cur.params.exp_time;
    const int cur_eg = status_cur.params.exp_gain;

    const double total_scale = brt_target / brt_cur;
    
    // Use adaptive weighted strategy to calculate scales
    auto [et_scale, eg_scale] = CalculateWeightedScales(total_scale, brt_target);

    #ifdef BUILD_WITH_LOG
        std::cout << "Adaptive weights - ET: " << et_weight << ", EG: " << eg_weight << std::endl;
        std::cout << "et scale " << et_scale << ",  eg scale " << eg_scale << std::endl;
    #endif

    int next_et = 0;
    if (min_et_step == 1) {
        next_et = static_cast<int>(cur_et * et_scale);
        next_et = std::clamp(next_et, min_et, max_et);
    } else {
        next_et = static_cast<int>((cur_et * et_scale) / min_et_step) * min_et_step;
        printf("cur_et %d, et_scale %f, next_et %d\n", cur_et, et_scale, next_et);
        if (next_et == cur_et) {
            if (et_scale < 1) {
                next_et -= min_et_step;  // ensure decrease one min_et_step
            } else if (et_scale > 1) {
                next_et += min_et_step;  // ensure increase one min_et_step
            }
            printf("Adjust et, cur_et %d, et_scale %f, next_et %d\n", cur_et, et_scale, next_et);
        }
        next_et = std::clamp(next_et, min_et_step, max_et);
    }

    int next_eg = static_cast<int>(cur_eg * eg_scale);
    next_eg = std::clamp(next_eg, min_eg, max_eg);
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

    params_next.exp_time = next_et;
    params_next.exp_gain = next_eg;
}

void AEImpl::CalculateMarginalEfficiency(int brt_target)
{
    // Add current values to history
    et_history.push_back(status_cur.params.exp_time);
    eg_history.push_back(status_cur.params.exp_gain);
    brt_history.push_back(status_cur.brt);
    
    // Keep history window size
    if (et_history.size() > history_window_size) {
        et_history.erase(et_history.begin());
        eg_history.erase(eg_history.begin());
        brt_history.erase(brt_history.begin());
    }
    
    // Calculate marginal efficiency only if we have enough history
    if (et_history.size() >= 2) {
        // Calculate changes
        double et_change = et_history.back() - et_history[et_history.size() - 2];
        double eg_change = eg_history.back() - eg_history[et_history.size() - 2];
        double brt_change = brt_history.back() - brt_history[brt_history.size() - 2];
        
        // Check if only one parameter changed significantly (for cleaner efficiency calculation)
        double et_change_pct = std::abs(et_change) / std::max(et_history[et_history.size() - 2], 1.0);
        double eg_change_pct = std::abs(eg_change) / std::max(eg_history[eg_history.size() - 2], 1.0);
        double brt_change_pct = std::abs(brt_change) / std::max(brt_history[brt_history.size() - 2], 1.0);
        
        // Calculate marginal efficiency based on dominant parameter change
        if (et_change_pct > eg_change_pct * 2.0 && et_change_pct > 1e-6) {
            // Exposure time was the dominant change
            marginal_et_efficiency = brt_change_pct / et_change_pct;
        } else if (eg_change_pct > et_change_pct * 2.0 && eg_change_pct > 1e-6) {
            // Gain was the dominant change
            marginal_eg_efficiency = brt_change_pct / eg_change_pct;
        } else {
            // Both parameters changed significantly, use theoretical models
            // For exposure time: assume linear relationship with diminishing returns
            double current_et = status_cur.params.exp_time;
            double normalized_et = (current_et - min_et) / (max_et - min_et);
            marginal_et_efficiency = std::exp(-normalized_et * 1.5); // Diminishing returns model
            marginal_et_efficiency = std::clamp(marginal_et_efficiency, 0.2, 1.0);
            
            // For gain: exponential decay due to noise amplification
            double current_gain = status_cur.params.exp_gain;
            double normalized_gain = (current_gain - min_eg) / (max_eg - min_eg);
            marginal_eg_efficiency = std::exp(-normalized_gain * 2.0);
            marginal_eg_efficiency = std::clamp(marginal_eg_efficiency, 0.1, 1.0);
        }
        
        #ifdef BUILD_WITH_LOG
            std::cout << "Marginal efficiency - ET: " << marginal_et_efficiency 
                      << ", EG: " << marginal_eg_efficiency 
                      << " (ET change: " << et_change_pct << "%, EG change: " << eg_change_pct << "%)" << std::endl;
        #endif
    }
}

void AEImpl::UpdateAdaptiveWeights()
{
    // Calculate efficiency ratio
    double efficiency_ratio = marginal_et_efficiency / (marginal_et_efficiency + marginal_eg_efficiency + 1e-6);
    
    // Update weights with smoothing factor
    double smoothing_factor = 0.3; // Default smoothing factor
    double new_et_weight = smoothing_factor * efficiency_ratio + (1 - smoothing_factor) * et_weight;
    double new_eg_weight = 1.0 - new_et_weight;
    
    // Apply constraints to prevent extreme values
    new_et_weight = std::clamp(new_et_weight, 0.2, 0.8); // Min/Max weight constraints
    new_eg_weight = std::clamp(new_eg_weight, 0.2, 0.8); // Min/Max weight constraints
    
    et_weight = new_et_weight;
    eg_weight = new_eg_weight;
    
    #ifdef BUILD_WITH_LOG
        std::cout << "Updated weights - ET: " << et_weight << ", EG: " << eg_weight 
                  << " (ET efficiency: " << marginal_et_efficiency 
                  << ", EG efficiency: " << marginal_eg_efficiency << ")" << std::endl;
    #endif
}

std::pair<double, double> AEImpl::CalculateWeightedScales(double total_scale, int brt_target)
{
    double et_scale = 1.0;
    double eg_scale = 1.0;
    
    if (total_scale < 1.0) {
        // Decreasing brightness - prioritize reducing gain first
        if (status_cur.params.exp_gain > init_eg) {
            eg_scale = static_cast<double>(init_eg) / status_cur.params.exp_gain;
        }
        et_scale = std::min(total_scale / eg_scale, 1.0);
        
        if (et_scale < 1 && status_cur.params.exp_time <= std::max(min_et, min_et_step)) {
            eg_scale = total_scale;
            et_scale = 1.0;
        }
    } else {
        // Increasing brightness - use adaptive weights
        double remaining_scale = total_scale;
        
        // First, try to use exposure time with its weight
        double et_scale_max = std::min(total_scale, static_cast<double>(max_et) / status_cur.params.exp_time);
        et_scale = 1.0 + (et_scale_max - 1.0) * et_weight;
        remaining_scale = total_scale / et_scale;
        
        // Then use gain for the remaining scale
        if (remaining_scale > 1.0) {
            eg_scale = remaining_scale;
            
            // Apply gain-specific constraints
            if (status_cur.params.exp_gain > 80 && status_cur.params.exp_gain <= 120) {
                eg_scale = std::min(1.3, eg_scale);
            } else if (status_cur.params.exp_gain > 120) {
                eg_scale = std::min(1.1, eg_scale);
            }
        }
    }
    
    return {et_scale, eg_scale};
}

void AEImpl::ResetHistory()
{
    et_history.clear();
    eg_history.clear();
    brt_history.clear();
    marginal_et_efficiency = 1.0;
    marginal_eg_efficiency = 1.0;
    et_weight = 0.7;
    eg_weight = 0.3;
    
    #ifdef BUILD_WITH_LOG
        std::cout << "Reset AE history and weights" << std::endl;
    #endif
}