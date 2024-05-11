#include "af_impl.h"
#include "af_config.h"
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

}


AFImpl::AFImpl(AFConf &af_conf, bool enable_hmap) {
    /* basic variables */
    this->end_iter = false;
    this->start_fit = false;
    this->next_pos = -1;
    this->best_pos = -1;

    /* variables for sampling */
    this->border_left = af_conf.START_POS;
    this->border_right = af_conf.END_POS;
    this->pos_start = border_left;
    this->pos_end = border_right;
    this->pos_cur_center = (border_left + border_right) / 2;
    this->pos_prev_center = -pos_cur_center;
    this->sampler.Init(border_left, border_right, af_alg_conf.max_n_samples);

    /* variables for heat-map */
    this->enable_hmap = enable_hmap;
    this->hmap_pos_step = af_alg_conf.hmap_pos_step;
    this->id_hots = {};
    this->best_hot.id = -1;

    /* variables for roi */
    this->enable_roi = false;
    this->roi = af_conf.ROI;

    /* misc */
    this->win_size = af_alg_conf.win_size;
    this->img_width = af_conf.IMG_WIDTH;
    this->img_height = af_conf.IMG_HEIGHT;
    this->pos_samples.clear();
    this->pos_with_fv.clear();
    this->pos_fv_recorder.clear();
    this->id_hots.clear();

    /* init the sampler and get the first sample */
    // ResetSamples();

    if(this->pos_start < 1)
    {
        this->pos_start = 1;
    }
    this->pos_samples = sampler.StepSampling(pos_start, pos_end, hmap_pos_step);  // 第一步的时候必须要使用step sampling，否则无法覆盖全区域
                                                                                  // step要自适应于min max focus，不同平台的min max差别很大
    this->fit_pos_num = 0;
    GetNextSample();
}

void AFImpl::Run(const cv::Mat &image, const std::vector<cv::Rect> &rois) {
    CalcFocusValue(image, rois);

    if (start_fit) {    
        this->RefineFocus();
    } 
    // else if (enable_hmap) {
    //     AnalysisHotspots();
    // } 
    else {
        AnalysisFvCurve();
    }

    GetNextSample();
    std::cout << " AFImpl::Run " << std::endl;
    std::cout << "next pos: " << this->next_pos << std::endl;
    std::cout << "remain pos_samples: ";
    for(auto ele : this->pos_samples)
    {
        std::cout << ele << ", ";
    }
    std::cout << std::endl;
}

void AFImpl::CalcFocusValue(const cv::Mat &image, const std::vector<cv::Rect> &rois) {
    std::vector<cv::Mat> roi_imgs;
    if(rois.empty())     // 没有传入roi，说明出于第一阶段调整，且barcode sdk没有传入roi，此时使用全图计算清晰度，默认resize以提高速度
    {
        cv::Mat resized_img;
        cv::resize(image, resized_img, cv::Size(), 0.5, 0.5);
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
        cv::Mat img_grad;
        cv::Sobel(roi_img, img_grad, CV_32FC1, 1, 1, 5);
        cv::Mat tmp1 = cv::abs(img_grad);
        sharpness_score += cv::sum(tmp1)[0];
    }

    this->pos_with_fv[next_pos] = sharpness_score;
    this->pos_fv_recorder[next_pos] = sharpness_score;
    printf("Focus pos=%d, focus value=%.2e\n", next_pos, sharpness_score);
}

void AFImpl::AnalysisHotspots() {
    if (id_hots.empty()) {
        /* Situation 1: No hotspot is detected */
        AnalysisFvCurve();
        return;
    }

    int peak_idx;
    std::vector<int> pos_vec;
    if (pos_samples.empty()) {
        /* Situation 2: All positions are sampled: apply max hotspot */
        best_hot = id_hots[0];  // id_hots has been sorted
        printf("hot intensities = (");
        for (const auto &intensity: best_hot.intensities) {
            printf("%.2e, ", intensity);
        }
        printf(")\n");
        ApplyMaxHot(pos_vec, peak_idx);
        UpdateStatus(pos_vec, peak_idx);
        printf("Apply max hotspot in pos %d\n", pos_cur_center);
        return;
    } else {
        /* Situation 3: Sampling in progress: search for peak in the identified hotspots */
        for (const auto &id_hot: id_hots) {
            pos_vec = id_hot.positions;
            std::vector<double> val_vec = id_hot.intensities;
            printf("hot intensities = (");
            for (const auto &intensity: id_hot.intensities) {
                printf("%.2e, ", intensity);
            }
            printf(")\n");
            peak_idx = SlidingWinSearch(val_vec, 7, false);
            if (peak_idx != -1) {
                best_hot = id_hot;
                UpdateStatus(pos_vec, peak_idx);
                printf("Found peak hotspot in pos %d\n", pos_cur_center);
                return;
            }
        }
    }
}

void AFImpl::AnalysisFvCurve() {
    if (pos_samples.empty()) {
        /* Situation 1: No sample is collected */
        std::vector<int> pos_vec;
        int peak_idx;
        ApplyMaxFv(pos_vec, peak_idx);
        std::cout << "find max fv in pos " << pos_vec[peak_idx] << std::endl;
        UpdateStatus(pos_vec, peak_idx);
        printf("Apply max fv in pos %d\n", pos_cur_center);
        this->best_pos = pos_cur_center;
    } else {
        /* Situation 2: Sampling in progress: search for peak in the focus value curve */
        std::vector<int> pos_vec;
        std::vector<double> fv_vec;
        GetPosFv(pos_vec, fv_vec);
        int peak_idx = SlidingWinSearch(fv_vec, win_size, false); // win_size default = 5

        if (fv_vec.size() > 1 && peak_idx < 0)
        {
            // 先判断是否是单调递减，按照现在的采样方法，主要的采样点都是在中间焦段，理论上fv_vec是二次凸函数
            // 但如果最佳的焦距落在全焦段的前端，那么这种采样方法下的fv_vec基本就是单调递减，需要提前退出后续采样
            // 如果fv_vec是一直单调递增的，就会正常走完后面的采样流程
            bool mono_decreasing = true;
            for(int i=1; i<fv_vec.size()-1; i++)
            {
                if(fv_vec[i] - fv_vec[i-1] > 0)
                {
                    mono_decreasing = false;
                    break;
                }
            }

            // 如果前一半都在单调递减，就直接跳出
            int total_sample_num = fv_vec.size() + this->pos_samples.size();
            float early_stop_ratio = 0.3;
            if (mono_decreasing && 
                fv_vec.size() >= total_sample_num * early_stop_ratio && 
                fv_vec.size() > 3)
            {
                std::cout << "triger mono decreasing early stop" << std::endl;
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
                    std::cout << "mono decreasing clear pos samples" << std::endl;
                }
            }
        }

        if (peak_idx != -1) {
            printf("Found peak fv in pos %d\n", pos_vec[peak_idx]);   // 在前面的判断中，如果不是单调递减的话，找到了peak之后，也会提前退出
            UpdateStatus(pos_vec, peak_idx);
            this->best_pos = pos_cur_center;
        }
    }
}

void AFImpl::PolynomialFit() {
    if (!pos_samples.empty()) {
        return;
    } else {
        end_iter = true;
    }

    /* Split the pos and fv to x, y */
    std::vector<int> x;
    std::vector<double> y;
    GetPosFv(x, y);
    for (auto &val: y) {
        val /= 1e10; // narrow the data range
    }

    /* Polynomial fit */
    cv::Mat A = cv::Mat::zeros(cv::Size(3, int(x.size())), CV_64FC1);
    for (int i = 0; i < x.size(); i++) {
        A.at<double>(i, 0) = 1;
        A.at<double>(i, 1) = x[i];
        A.at<double>(i, 2) = x[i] * x[i];
    }

    cv::Mat b = cv::Mat::zeros(cv::Size(1, int(y.size())), CV_64FC1);
    for (int i = 0; i < y.size(); i++) {
        b.at<double>(i, 0) = y[i];
    }

    cv::Mat c;
    c = A.t() * A;
    cv::Mat d;
    d = A.t() * b;

    cv::Mat result = cv::Mat::zeros(cv::Size(1, 3), CV_64FC1);
    cv::solve(c, d, result);
    double a1 = result.at<double>(1, 0);
    double a2 = result.at<double>(2, 0);

    /* Calculate the peak position */
    int poly_max_pos = static_cast<int>(round(-a1 / (2 * a2)));
    int y_max_pos = x[std::max_element(y.begin(), y.end()) - y.begin()];
    if (abs(poly_max_pos - y_max_pos) <= 2) {
        best_pos = poly_max_pos;
    } else {
        best_pos = y_max_pos;
    }
    printf("Polynomial fit: poly_max_pos=%d, y_max_pos=%d\n", poly_max_pos, y_max_pos);
}

void AFImpl::RefineFocus()
{
    if (!this->pos_samples.empty()) {  // 如果this->pos_samples里面还有数据，说明这一轮还没拍完照，还不需要处理
        return;
    } else {      // 这一轮拍完照后，开始拟合二次函数,af结束
        end_iter = true;
    }

    std::vector<int> sampling_pos;
    std::vector<double> sampling_sharpness;
    GetPosFv(sampling_pos, sampling_sharpness);

    // 有足够多点的时候，使用拟合二次函数求最高点，否则直接找最大值
    if(sampling_sharpness.size() > this->fit_pos_num / 2 + 1)
    {
        int min_limit_pos = *std::min_element(sampling_pos.begin(), sampling_pos.end());
        int max_limit_pos = *std::max_element(sampling_pos.begin(), sampling_pos.end());
        std::cout << "fit quadratic" << std::endl;
        std::vector<double> coeffs = FitQuadratic(sampling_pos, sampling_sharpness);
        this->best_pos = -coeffs[1] / (2 * coeffs[0]);
        if (this->best_pos < min_limit_pos)
        {
            std::cout << "smaller than min_limit, pre is " << this->best_pos << std::endl;
            this->best_pos = min_limit_pos;
        }
        else if(this->best_pos > max_limit_pos)
        {
            std::cout << "greater than max_limit, pre is " << this->best_pos << std::endl;
            this->best_pos = max_limit_pos;
        }
        double peak_sharpness = coeffs[0] * best_pos * best_pos + coeffs[1] * best_pos + coeffs[2];
        std::cout << "RefineFocus fit best_pos: " << best_pos << ", sharpness score: " << peak_sharpness << std::endl;
    }
    else
    {
        auto max_sharpness_it = std::max_element(sampling_sharpness.begin(), sampling_sharpness.end());
        int max_sharpness_idx = std::distance(sampling_sharpness.begin(), max_sharpness_it);
        this->best_pos = sampling_pos[max_sharpness_idx];
        std::cout << "RefineFocus find best_pos: " << this->best_pos << std::endl;
    }

    // std::cout << "sampling_sharpness.size() " << sampling_sharpness.size() << ",  " << this->fit_pos_num << std::endl;
    // if (sampling_sharpness.size() <= this->fit_pos_num / 2 + 1)
    // {
    //     std::cout << "too few sampling_sharpness" << std::endl;
    //     auto max_sharpness_it = std::max_element(sampling_sharpness.begin(), sampling_sharpness.end());
    //     int max_sharpness_idx = std::distance(sampling_sharpness.begin(), max_sharpness_it);
    //     this->best_pos = sampling_pos[max_sharpness_idx];
    //     std::cout << "RefineFocus find best_pos: " << this->best_pos << std::endl;
    // }

    std::cout << "RefineFocus finish" << std::endl;
}

void AFImpl::UpdateStatus(std::vector<int> &pos_vec, int peak_idx) {
    /* Check if the peak is found */
    if (peak_idx == -1) {
        return;
    }

    /* Update current and previous center, adjust the search range(pos_start, pos_end) */
    pos_prev_center = pos_cur_center;
    pos_cur_center = pos_vec[peak_idx];
    ElasticRangeAdjust(pos_vec, peak_idx);

    /* Disable hmap and enable roi if the best hot is found */
    if (enable_hmap) {
        if (best_hot.id != -1) {
            roi = ExpandRect(best_hot.rect);
            enable_roi = true;
            pos_fv_recorder.clear();  // clear the recorder (the values are invalid)
        }
        enable_hmap = false;
    }

    /* Check if the range is narrow enough or the peak is stable to start the polynomial fitting*/
    if (abs(pos_start - pos_end) < 6 ||
        abs(pos_cur_center - pos_prev_center) < 6 ||
        abs(pos_cur_center - pos_start) < 6 ||
        abs(pos_cur_center - pos_end) < 6) {
        start_fit = true;  // TODO: 20, 6 is a magic number
        std::cout << "set start_fit true" << std::endl;
        // end_iter = true;
        // pos_samples.clear();
        // return;
    }

    /* Reset the samples */
    ResetSamples();
}

void AFImpl::ResetSamples() {
    /* Clear the previous samples queue and its corresponding focus values */
    pos_samples.clear();
    pos_with_fv.clear();

    /* Sample the position */
    if (start_fit) {
        // printf("Fitting sampling: start = %d, end = %d, step = %d\n", pos_cur_center-6, pos_cur_center+6, 2);
        // pos_samples = sampler.StepSampling(pos_cur_center - 6, pos_cur_center + 6, 2);
        const int range = 3;
        const int step = 5;
        printf("Fitting sampling: start = %d, end = %d, step = %d\n", pos_cur_center - range * step, pos_cur_center + range * step, step);
        pos_samples = sampler.StepSampling(pos_cur_center - range*step, pos_cur_center + range*step, step);
        
        auto it = std::find(this->pos_samples.begin(), this->pos_samples.end(), pos_cur_center);
        if(it != this->pos_samples.end())
        {
            this->pos_samples.erase(it);
            // 把这个值移动到第一个位置，算法流程会先拍这个位置，
            // 在AT4VsImpl::SequentialExec()的AF phase中，会先拍这个焦距的图跑解码函数，用来定位码区
            this->pos_samples.insert(this->pos_samples.begin(), pos_cur_center);  
        }
        printf("StepSampling, samples: ");
        for (int sample : this->pos_samples) {
            printf("%d ", sample);
        }
        printf("\n");

        this->fit_pos_num = range * 2 + 1;
    } else if (enable_hmap) {
        // printf("HMap sampling: start = %d, end = %d, step = %d\n", pos_start, pos_end, hmap_pos_step);
        pos_samples = sampler.StepSampling(pos_start, pos_end, hmap_pos_step);
    } else {
        printf("sparse sampling: start = %d, end = %d, center = %d\n", pos_start, pos_end, pos_cur_center);
        pos_samples = sampler.SparseSampling(pos_start, pos_end, pos_cur_center);
    }
    win_size = std::max(3, static_cast<int>(pos_samples.size() / 2));

    /* If pos has been sampled before, add it to the pos_with_fv and remove it from the pos_samples*/
    // for (auto it = pos_samples.begin(); it != pos_samples.end();) {
    //     if (pos_fv_recorder.find(*it) != pos_fv_recorder.end()) {
    //         pos_with_fv[*it] = pos_fv_recorder[*it];
    //         it = pos_samples.erase(it);
    //         win_size += 1;
    //     } else {
    //         it++;
    //     }
    // }
}

void AFImpl::GetNextSample() {
    if (!pos_samples.empty()) {
        next_pos = pos_samples.front();   // 取位置时，从头开始拿
        pos_samples.erase(pos_samples.begin());
    } else {
        next_pos = best_pos;
    }
}

int AFImpl::SlidingWinSearch(std::vector<double> &val_vec, int win_size, bool mono_check) {
    int peak_idx = -1;

    /* Check if the number of samples is enough */
    int n = static_cast<int>(val_vec.size());
    if (n < win_size) {
        return peak_idx;
    }

    double cur_max = *std::max_element(val_vec.begin(), val_vec.end());
    double peak_thre = cur_max * 2 / 3;  //如果找到的peak无法大于最大值的2/3，则有可能是个毛刺

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
    for (int i = 1; i < n - 1; i++) {
        bool is_peak = true;
        for (int offset = -r; offset <= r; offset++) {
            if (i + offset < 0 || i + offset >= n || offset == 0) {
                continue;
            }
            if (val_vec[i] <= val_vec[i + offset]) {
                is_peak = false;
                break;
            }
        }
        if (is_peak && val_vec[i] > peak_thre) {
            peak_idx = i;
            break;
        }
    }

    return peak_idx;
}

void AFImpl::ApplyMaxHot(std::vector<int> &pos_vec, int &peak_idx) const {
    // get pos and val vectors
    pos_vec = best_hot.positions;
    std::vector<double> val_vec = best_hot.intensities;

    // peak index is the index of the max intensity
    peak_idx = static_cast<int>(
            std::max_element(val_vec.begin(), val_vec.end()) - val_vec.begin());
}

void AFImpl::ApplyMaxFv(std::vector<int> &pos_vec, int &peak_idx) {
    // get pos and fv vectors
    pos_vec;
    std::vector<double> fv_vec;
    GetPosFv(pos_vec, fv_vec);

    // peak index is the index of the max fv
    peak_idx = static_cast<int>(
            std::max_element(fv_vec.begin(), fv_vec.end()) - fv_vec.begin());
}

void AFImpl::ElasticRangeAdjust(std::vector<int> &pos_vec, int peak_idx) {
    if (peak_idx > 0) {
        pos_start = pos_vec[peak_idx - 1];
    } else {
        int interval = pos_vec[peak_idx + 1] - pos_vec[peak_idx];
        pos_start = std::max(border_left, pos_vec[peak_idx] - interval*5);
    }
    if (peak_idx < pos_vec.size() - 1) {
        pos_end = pos_vec[peak_idx + 1];
    } else {
        int interval = pos_vec[peak_idx] - pos_vec[peak_idx - 1];
        pos_end = std::min(border_right, pos_vec[peak_idx] + interval*5);
    }
}

void AFImpl::GetPosFv(std::vector<int> &pos_vec, std::vector<double> &fv_vec) {
    pos_vec.clear();
    fv_vec.clear();
    for (auto &pos_fv: this->pos_with_fv) {
        pos_vec.push_back(pos_fv.first);
        fv_vec.push_back(pos_fv.second);
    }
}

cv::Rect2d AFImpl::ExpandRect(const cv::Rect2d &rect) const {
    cv::Rect2d expand_rect;
    expand_rect.x = rect.x - rect.width * (2 - 1) / 2;
    expand_rect.y = rect.y - rect.height * (2 - 1) / 2;
    expand_rect.width = rect.width * 2;
    expand_rect.height = rect.height * 2;
    // boundary check
    if (expand_rect.x < 0) {
        expand_rect.x = 0;
    }
    if (expand_rect.y < 0) {
        expand_rect.y = 0;
    }
    if (expand_rect.x + expand_rect.width > img_width) {
        expand_rect.width = img_width - expand_rect.x;
    }
    if (expand_rect.y + expand_rect.height > img_height) {
        expand_rect.height = img_height - expand_rect.y;
    }
    return expand_rect;
}
