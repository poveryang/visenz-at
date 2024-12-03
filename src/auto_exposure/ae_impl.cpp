#include "ae_impl.h"
#include <string>
#include <numeric>
#include <algorithm>

using namespace ae;

int ae_save_count = 0;

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
            this->lights_sets = {unpolarized, polarized, all};
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
    this->exceed_tunning_count = false;   
}

bool AEImpl::QuickTune(const cv::Mat &image, int brt_target, const std::vector<cv::Rect> &rois, int brt_diff_thre, bool enable_switch, float fraction) 
{
    /* Calc metrics in current frame */
    #ifdef BUILD_WITH_LOG
        std::cout << "ae brt_target " << brt_target << ", brt_diff_thre" << brt_diff_thre << std::endl;
    #endif
    CalcMetrics(image, rois, brt_target);
    this->tuning_count += 1;
    #ifdef BUILD_WITH_LOG
        std::cout << "ae tuning count " << this->tuning_count << std::endl;
    #endif

    if (abs(status_cur.brt - brt_target) < brt_diff_thre) 
    {
        params_best = params_next;
        #ifdef BUILD_WITH_LOG
            std::cout << "reach giving brt, reset tuning_count = 0" << std::endl;
        #endif
        this->tuning_count = 0;
        return true;
    } 
    else 
    {
        if(this->tuning_count > this->max_tuning_count)
        {
            this->exceed_tunning_count = true;
            this->tuning_count = 0;
            if(!enable_switch)
            {
                this->ae_fail = true;
            }
            return false;

            /*
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
            */
        }

        this->UpdateExposureAndGain(brt_target, fraction);

        return false;
    }
}

bool AEImpl::UpdateLights()
{
    this->tuning_count = 0;
    this->exceed_tunning_count = false;
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

void AEImpl::CalcMetrics(const cv::Mat &image, const std::vector<cv::Rect> &rois, int brt_target) 
{
    /* Calc metrics */
    double brt =-1;
    brt = CalcMeanBrt(image, rois, brt_target);

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

void AEImpl::UpdateExposureAndGain(int brt_target, float fraction)
{
    #ifdef BUILD_WITH_LOG
        std::cout << "UpdateExposureAndGain()" << std::endl;
    #endif
    float brt_cur = static_cast<float>(this->status_cur.brt);
    brt_cur = std::max(brt_cur, 1.0f);  // 防止除零
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
        if(cur_exposure * total_scale > (this->max_et*fraction))
        {
            et_scale = (this->max_et*fraction) * 1.0f / cur_exposure;
            eg_scale = std::min(2.0f, total_scale / et_scale);
        }
        if(cur_eg > 80 && cur_eg <=120)   // 实验表明，亮度曲线是分段的，统一设置当超过80后，限制放大系数，以免超调
        {
            eg_scale = std::min(1.1f, eg_scale);
        }
        else if(cur_eg > 120)
        {
            eg_scale = std::min(1.05f, eg_scale);
        }
    }

    #ifdef BUILD_WITH_LOG
        std::cout << "et scale " << et_scale << ",  eg scale" << eg_scale << std::endl; 
    #endif

    int next_et = std::min(int(cur_exposure*et_scale), int(this->max_et*fraction));
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