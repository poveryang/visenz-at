#include "af_impl.h"
#include <chrono>

using namespace af;

namespace
{

std::vector<double> FitQuadratic(const std::vector<int> &x, const std::vector<double> &y)
{
    cv::Mat A = cv::Mat::zeros(cv::Size(3, int(x.size())), CV_64FC1);
    for (int i = 0; i < x.size(); i++) {
        A.at<double>(i, 0) = x[i] * x[i];
        A.at<double>(i, 1) = x[i];
        A.at<double>(i, 2) = 1;
    }

    cv::Mat B = cv::Mat::zeros(cv::Size(1, int(y.size())), CV_64FC1);
    for (int i = 0; i < y.size(); i++) {
        B.at<double>(i, 0) = y[i];
    }

    cv::Mat tmp1 = A.t() * A;
    cv::Mat tmp2 = A.t() * B;

    cv::Mat result = cv::Mat::zeros(cv::Size(1, 3), CV_64FC1);
    cv::solve(tmp1, tmp2, result);
    double a = result.at<double>(0, 0);
    double b = result.at<double>(1, 0);
    double c = result.at<double>(2, 0);

    return {a, b, c};   // quadratic formula: y = a*x^2 + b*x + c
}

} // namespace


AFImpl::AFImpl(AFConf &af_conf) 
{
    /* basic variables */
    this->end_iter = false;
    this->start_fit = false;
    this->next_pos = -1;
    this->best_pos = -1;

    /* variables for sampling */
    this->border_left = af_conf.start_pos;
    this->border_right = af_conf.end_pos;
    this->pos_start = border_left;
    this->pos_end = border_right;
    this->pos_cur_center = (border_left + border_right) / 2;
    this->pos_prev_center = -pos_cur_center;
    this->sampler.Init(border_left, border_right, 10);

    this->coarse_step = af_conf.coarse_step;
    this->refine_step = af_conf.refine_step;
    this->refine_range = 3;

    /* misc */
    this->win_size = 5;
    this->pos_samples.clear();
    this->pos_with_fv.clear();

    if(this->pos_start < 1)
    {
        this->pos_start = 1;
    }

    this->first_stage_ = true;
    this->pos_samples = sampler.StepSampling(pos_start, pos_end, this->coarse_step);  // 第一步的时候必须要使用step sampling，否则无法覆盖全区域
    this->fit_pos_num_ = 0;

    GetNextSample();
}

void AFImpl::Run(const cv::Mat &image, const std::vector<cv::Rect> &rois) 
{
    CalcFocusValue(image, rois);

    if (this->start_fit) 
    {    
        this->RefineFocus();
    } 
    else 
    {
        AnalysisFvCurve();
    }

    GetNextSample();
    #ifdef BUILD_WITH_LOG
        std::cout << " AFImpl::Run " << std::endl;
        std::cout << "next pos: " << this->next_pos << std::endl;
        std::cout << "remain pos_samples: ";
        for(auto ele : this->pos_samples)
        {
            std::cout << ele << ", ";
        }
        std::cout << std::endl;
    #endif
}

void AFImpl::CalcFocusValue(const cv::Mat &image, const std::vector<cv::Rect> &rois) 
{
    std::vector<cv::Mat> roi_imgs;
    if(rois.empty())     // 没有传入roi，说明出于第一阶段调整，且barcode sdk没有传入roi，此时使用全图计算清晰度，默认resize以提高速度
    {
        cv::Mat resized_img;
        float scale_ratio = 0.5;
        if (image.rows > 2500)
        {
            scale_ratio = 0.25;
        }
        cv::resize(image, resized_img, cv::Size(), scale_ratio, scale_ratio, cv::INTER_NEAREST);
        roi_imgs.emplace_back(resized_img);
    }
    else     // 当传入roi时，只处理roi的内容，如果roi过大，默认resize以提高速度
    {
        for(const auto &region : rois)
        {
            cv::Mat roi_img = image(region);

            if (roi_img.cols > 1280 || roi_img.rows > 800) 
            {
                float scale = std::min(1280.0 / roi_img.cols, 800.0 / roi_img.rows);
                cv::resize(roi_img, roi_img, cv::Size(), scale, scale);
            }
            roi_imgs.emplace_back(roi_img);
        }
    }

    double sharpness_score = 0;
    for(const auto &roi_img : roi_imgs)
    {
        // cv::Mat img_grad;
        // cv::Sobel(roi_img, img_grad, CV_32FC1, 1, 1, 5);
        // cv::Mat tmp1 = cv::abs(img_grad);
        // sharpness_score += cv::sum(tmp1)[0];
        
        cv::Mat img_grad_x, img_grad_y;
        cv::Sobel(roi_img, img_grad_x, CV_32FC1, 1, 0, 5);
        cv::Sobel(roi_img, img_grad_y, CV_32FC1, 0, 1, 5);

        int min_grad = 15;       // 这里需要实验，在coarse和fit阶段，是否需要使用不同thre, coarse阶段用更大的thre
        cv::Mat thre_map_x, thre_map_y;
        cv::threshold(img_grad_x, thre_map_x, min_grad, 1, cv::THRESH_BINARY);
        cv::threshold(img_grad_y, thre_map_y, min_grad, 1, cv::THRESH_BINARY);
        cv::multiply(img_grad_x, thre_map_x, img_grad_x);
        cv::multiply(img_grad_y, thre_map_y, img_grad_y);

        cv::Mat tmp1 = cv::abs(img_grad_x) + cv::abs(img_grad_y);
        sharpness_score += cv::sum(tmp1)[0];
    }
    if (!rois.empty())
    {
        sharpness_score /= rois.size();
    }
    
    this->pos_with_fv[next_pos] = sharpness_score;
    #ifdef BUILD_WITH_LOG
        printf("Focus pos=%d, focus value=%.2e\n", next_pos, sharpness_score);
    #endif
}

void AFImpl::AnalysisFvCurve() 
{
    if (pos_samples.empty()) 
    {
        /* Situation 1: No sample is collected */
        std::vector<int> pos_vec;
        int peak_idx;
        ApplyMaxFv(pos_vec, peak_idx);
        #ifdef BUILD_WITH_LOG
            std::cout << "find max fv in pos " << pos_vec[peak_idx] << std::endl;
        #endif
        UpdateStatus(pos_vec, peak_idx);
        #ifdef BUILD_WITH_LOG
            printf("Apply max fv in pos %d\n", pos_cur_center);
        #endif
        this->best_pos = pos_cur_center;
    } 
    else 
    {
        /* Situation 2: Sampling in progress: search for peak in the focus value curve */
        std::vector<int> pos_vec;
        std::vector<double> fv_vec;
        GetPosFv(pos_vec, fv_vec);
        #ifdef BUILD_WITH_LOG
            std::cout << "fv_vec: ";
            for(auto ele : fv_vec)
            {
                std::cout << ele << ",";
            }
            std::cout << std::endl;
        #endif

        int peak_idx = SlidingWinSearch(fv_vec, win_size, false); // win_size default = 5

        if (fv_vec.size() > 1 && peak_idx < 0)
        {
            // 先判断是否是单调递减，按照现在的采样方法，主要的采样点都是在中间焦段，理论上fv_vec是二次凸函数
            // 但如果最佳的焦距落在全焦段的前端，那么这种采样方法下的fv_vec基本就是单调递减，需要提前退出后续采样
            // 如果fv_vec是一直单调递增的，就会正常走完后面的采样流程
            bool mono_decreasing = true;
            for(int i=1; i<fv_vec.size(); i++)
            {
                if(fv_vec[i] - fv_vec[i-1] > 0)
                {
                    mono_decreasing = false;
                    break;
                }
            }
            if (mono_decreasing)
            {
                //假如是真的单调递减的曲线，front的值会比back的值高很多，进不了这个if
                if (fv_vec.front() < fv_vec.back() * 1.5)  
                {
                    mono_decreasing = false;
                }
            }

            // 如果前一半都在单调递减，就直接跳出
            int total_sample_num = fv_vec.size() + this->pos_samples.size();
            float early_stop_ratio = 0.3;
            if (mono_decreasing && 
                fv_vec.size() >= total_sample_num * early_stop_ratio && 
                fv_vec.size() > 3)
            {
                #ifdef BUILD_WITH_LOG
                    std::cout << "triger mono decreasing early stop" << std::endl;
                #endif
                this->best_pos = pos_vec[0];
                UpdateStatus(pos_vec, 0);
                if(!this->start_fit)
                {
                    std::vector<int> tmp_pos_samples(this->pos_samples.begin(), this->pos_samples.end());
                    this->pos_samples.clear();
                    for(auto ele : tmp_pos_samples)
                    {
                        if(ele < pos_vec[0])
                        {
                            this->pos_samples.emplace_back(ele);
                        }
                    }
                    #ifdef BUILD_WITH_LOG
                        std::cout << "mono decreasing clear pos samples" << std::endl;
                    #endif
                }
            }
        }

        if (peak_idx != -1) 
        {
            #ifdef BUILD_WITH_LOG
                printf("Found peak fv in pos %d\n", pos_vec[peak_idx]);   // 在前面的判断中，如果不是单调递减的话，找到了peak之后，也会提前退出
            #endif
            UpdateStatus(pos_vec, peak_idx);
            this->best_pos = pos_cur_center;
        }
    }
}

void AFImpl::RefineFocus()
{
    if (!this->pos_samples.empty()) 
    {  // 如果this->pos_samples里面还有数据，说明这一轮还没拍完照，还不需要处理
        return;
    } 
    else 
    {      // 这一轮拍完照后，开始拟合二次函数,af结束
        end_iter = true;
    }

    std::vector<int> sampling_pos;
    std::vector<double> sampling_sharpness;
    GetPosFv(sampling_pos, sampling_sharpness);

    #ifdef BUILD_WITH_LOG
        std::cout << "AFImpl::RefineFocus() ";
        for(auto ele : sampling_sharpness)
        {
            std::cout << ele << ", ";
        }
        std::cout << std::endl;
    #endif

    // 有足够多点的时候，使用拟合二次函数求最高点，否则直接找最大值
    if(sampling_sharpness.size() > this->fit_pos_num_ / 2 + 1)
    {
        int min_limit_pos = *std::min_element(sampling_pos.begin(), sampling_pos.end());
        int max_limit_pos = *std::max_element(sampling_pos.begin(), sampling_pos.end());
        #ifdef BUILD_WITH_LOG
            std::cout << "fit quadratic" << std::endl;
        #endif
        std::vector<double> coeffs = FitQuadratic(sampling_pos, sampling_sharpness);
        this->best_pos = -coeffs[1] / (2 * coeffs[0]);
        if (this->best_pos < min_limit_pos)
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "smaller than min_limit, pre is " << this->best_pos << std::endl;
            #endif
            this->best_pos = min_limit_pos;
        }
        else if(this->best_pos > max_limit_pos)
        {
            #ifdef BUILD_WITH_LOG
                std::cout << "greater than max_limit, pre is " << this->best_pos << std::endl;
            #endif
            this->best_pos = max_limit_pos;
        }
        double fit_peak_sharpness = coeffs[0] * best_pos * best_pos + coeffs[1] * best_pos + coeffs[2];
        #ifdef BUILD_WITH_LOG
            std::cout << "RefineFocus fit best_pos: " << best_pos << ", fit sharpness score: " << fit_peak_sharpness << std::endl;
        #endif

        const float using_thresh = 0.95;  // 若已有的最高值乘以这个thre后，仍然比fit出来的peak要高，说明计算二次函数可能出错，那么就使用find出来最高值
        auto find_max_sharpness_it = std::max_element(sampling_sharpness.begin(), sampling_sharpness.end());
        if ((*find_max_sharpness_it)*using_thresh > fit_peak_sharpness)
        {
            int max_sharpness_idx = std::distance(sampling_sharpness.begin(), find_max_sharpness_it);
            this->best_pos = sampling_pos[max_sharpness_idx];
            #ifdef BUILD_WITH_LOG
                std::cout << "fit quadratic error, using found peak" << std::endl;
                std::cout << "found best_pos " << this->best_pos << ", found sharpness score: " << *find_max_sharpness_it << std::endl;
            #endif
        }
    }
    else
    {
        auto max_sharpness_it = std::max_element(sampling_sharpness.begin(), sampling_sharpness.end());
        int max_sharpness_idx = std::distance(sampling_sharpness.begin(), max_sharpness_it);
        this->best_pos = sampling_pos[max_sharpness_idx];
        #ifdef BUILD_WITH_LOG
            std::cout << "RefineFocus find best_pos: " << this->best_pos << std::endl;
        #endif
    }

    this->pos_cur_center = this->best_pos;

    #ifdef BUILD_WITH_LOG
        std::cout << "update this->pos_cur_center = " << this->pos_cur_center << std::endl;
        std::cout << "RefineFocus finish" << std::endl;
    #endif
}

void AFImpl::UpdateStatus(std::vector<int> &pos_vec, int peak_idx) 
{
    /* Check if the peak is found */
    if (peak_idx == -1) 
    {
        return;
    }

    /* Update current and previous center, adjust the search range(pos_start, pos_end) */
    pos_prev_center = pos_cur_center;
    pos_cur_center = pos_vec[peak_idx];
    ElasticRangeAdjust(pos_vec, peak_idx);
    
    #ifdef BUILD_WITH_LOG
        std::cout << "AFImpl::UpdateStatus()" << std::endl;
        std::cout << "[pos_start, pos_cur_center, pos_end]: [" << pos_start << ", " << pos_cur_center << ", " << pos_end << "]" << std::endl;
    #endif

    /* Check if the range is narrow enough or the peak is stable to start the polynomial fitting*/
    if (abs(pos_start - pos_end) < this->refine_step * this->refine_range ||
        abs(pos_cur_center - pos_prev_center) < this->refine_step * this->refine_range ||
        abs(pos_cur_center - pos_start) < this->refine_step * this->refine_range ||
        abs(pos_cur_center - pos_end) < this->refine_step * this->refine_range) 
    {
        this->start_fit = true;  
        #ifdef BUILD_WITH_LOG
            std::cout << "set start_fit true" << std::endl;
        #endif
    }
    else
    {
        #ifdef BUILD_WITH_LOG
            std::cout << "AFImpl::UpdateStatus could not fall in narrow range, resample again" << std::endl;
        #endif
    }

    /* Reset the samples */
    ResetSamples();
}

void AFImpl::ResetSamples() 
{
    /* Clear the previous samples queue and its corresponding focus values */
    pos_samples.clear();
    pos_with_fv.clear();
    #ifdef BUILD_WITH_LOG
        std::cout << "AFImpl::ResetSamples start_fit " << start_fit << std::endl;
    #endif
    /* Sample the position */
    if (this->start_fit) 
    {
        const int range = this->refine_range;
        const int step = this->refine_step;
        #ifdef BUILD_WITH_LOG
            printf("Fitting sampling: start = %d, end = %d, step = %d\n", pos_cur_center - range * step, pos_cur_center + range * step, step);
        #endif
        pos_samples = sampler.StepSampling(pos_cur_center - range*step, pos_cur_center + range*step, step);
        this->pos_start = pos_cur_center - range*step;
        this->pos_end = pos_cur_center + range*step;
        
        auto it = std::find(this->pos_samples.begin(), this->pos_samples.end(), pos_cur_center);
        if(it != this->pos_samples.end())
        {
            this->pos_samples.erase(it);
            // 把这个值移动到第一个位置，算法流程会先拍这个位置，
            // 在AT4VsImpl::SequentialExec()的AF phase中，会先拍这个焦距的图跑解码函数，用来定位码区
            this->pos_samples.insert(this->pos_samples.begin(), pos_cur_center);  
        }
        #ifdef BUILD_WITH_LOG
            printf("StepSampling, samples: ");
            for (int sample : this->pos_samples) {
                printf("%d ", sample);
            }
            printf("\n");
        #endif

        this->fit_pos_num_ = range * 2 + 1;
    } 
    else 
    {
        #ifdef BUILD_WITH_LOG
            printf("sparse sampling: start = %d, end = %d, center = %d\n", pos_start, pos_end, pos_cur_center);
        #endif
        pos_samples = sampler.SparseSampling(pos_start, pos_end, pos_cur_center);
    }
    win_size = std::max(3, static_cast<int>(pos_samples.size() / 2));
}

void AFImpl::GetNextSample() 
{
    if (!pos_samples.empty()) 
    {
        next_pos = pos_samples.front();   // 取位置时，从头开始拿
        pos_samples.erase(pos_samples.begin());
    } 
    else 
    {
        next_pos = best_pos;
    }
}

int AFImpl::SlidingWinSearch(std::vector<double> &val_vec, int win_size, bool mono_check) 
{
    int peak_idx = -1;

    /* Check if the number of samples is enough */
    int n = static_cast<int>(val_vec.size());
    if (n < win_size) 
    {
        return peak_idx;
    }

    double cur_max = *std::max_element(val_vec.begin(), val_vec.end());
    double peak_thre = cur_max * 2 / 3;  //如果找到的peak无法大于最大值的2/3，则有可能是个毛刺

    double found_peak_thre_ratio = 1.1;
    // 对于第一次的对焦，清晰的地方会比周围高很多才对
    if(this->first_stage_)
    {
        found_peak_thre_ratio = 1.2;
    }

    /* Monotonicity check (when all the samples are collected) */
    if (mono_check) {
        bool mono_dec = val_vec[0] > val_vec[1];          // monotonicity decreasing on the left side
        bool mono_inc = val_vec[n - 1] > val_vec[n - 2];      // monotonicity increasing on the right side
        for (int i = 1; i < win_size - 1; i++) {
            if (val_vec[i] < val_vec[i + 1]) {
                mono_dec = false;
            }
            if (val_vec[n - i - 1] < val_vec[n - i - 2]) {
                mono_inc = false;
            }
        }
        if (mono_dec) {
            peak_idx = 0;
        } else if (mono_inc) {
            peak_idx = n - 1;
        }
    }


    /* Search for the peak in the sliding window */
    int r = (win_size - 1) / 2;
    for (int i = 1; i < n - 1; i++) 
    {
        bool is_peak = true;
        for (int offset = -r; offset <= r; offset++) 
        {
            if (i + offset < 0 || i + offset >= n || offset == 0) 
            {
                continue;
            }
            if (val_vec[i] <= val_vec[i + offset]*found_peak_thre_ratio) 
            {
                is_peak = false;
                break;
            }
        }

        if (is_peak && val_vec[i] > peak_thre) 
        {
            this->first_stage_ = false;
            peak_idx = i;
            break;
        }
    }

    return peak_idx;
}

void AFImpl::ApplyMaxFv(std::vector<int> &pos_vec, int &peak_idx) 
{
    // get pos and fv vectors
    pos_vec;
    std::vector<double> fv_vec;
    GetPosFv(pos_vec, fv_vec);

    #ifdef BUILD_WITH_LOG
        std::cout << "AFImpl::ApplyMaxFv" << std::endl;
        std::cout << "fv_vec: ";
        for(auto ele : fv_vec)
        {
            std::cout << ele << ",";
        }
        std::cout << std::endl;
    #endif

    // peak index is the index of the max fv
    peak_idx = static_cast<int>(
            std::max_element(fv_vec.begin(), fv_vec.end()) - fv_vec.begin());
}

void AFImpl::ElasticRangeAdjust(std::vector<int> &pos_vec, int peak_idx) 
{
    if (peak_idx > 0) 
    {
        pos_start = pos_vec[peak_idx - 1];
    } 
    else 
    {
        int interval = pos_vec[peak_idx + 1] - pos_vec[peak_idx];
        pos_start = std::max(border_left, pos_vec[peak_idx] - interval*5);
    }
    if (peak_idx < pos_vec.size() - 1) 
    {
        pos_end = pos_vec[peak_idx + 1];
    } 
    else 
    {
        int interval = pos_vec[peak_idx] - pos_vec[peak_idx - 1];
        pos_end = std::min(border_right, pos_vec[peak_idx] + interval*5);
    }
}

void AFImpl::GetPosFv(std::vector<int> &pos_vec, std::vector<double> &fv_vec) 
{
    pos_vec.clear();
    fv_vec.clear();
    for (auto &pos_fv: this->pos_with_fv) 
    {
        pos_vec.push_back(pos_fv.first);
        fv_vec.push_back(pos_fv.second);
    }
}
