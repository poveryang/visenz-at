#include "ae_impl.h"

using namespace ae;

AEImpl::AEImpl(const AEConf &ae_conf, bool en_al) {
    /* Camera config related to AE */
    MODE = ae_conf.ae_mode;
    MIN_ET = ae_conf.min_et;
    MAX_ET = ae_conf.max_et;
    MIN_EG = ae_conf.min_eg;
    MAX_EG = ae_conf.max_eg;
    EG_QUANT_SCALE = ae_conf.eg_quant_scale;
    MAX_LT = ae_conf.max_lt;
    MIN_INTENSITY = ae_conf.min_intensity;
    MAX_INTENSITY = ae_conf.max_intensity;
    IMG_WIDTH = ae_conf.img_width;
    IMG_HEIGHT = ae_conf.img_height;

    /* Basic variables */
    num_qt = 0;
    max_num_qt = 7;
    oob_max = oob_min = false;
    status_prev.brt = status_cur.brt = -1;
    params_next.exp_time = MAX_LT;
    params_next.exp_gain = MIN_EG;
    if (en_al) {
        std::vector<int> all;
        std::vector<int> polarized;
        std::vector<int> unpolarized;
        size_t n_lights = ae_conf.init_intensities.size();
        for (size_t i = 0; i < n_lights; i++) {
            all.emplace_back(MAX_INTENSITY);
            if (i < n_lights / 2) {
                polarized.emplace_back(0);
                unpolarized.emplace_back(MAX_INTENSITY);
            } else {
                polarized.emplace_back(MAX_INTENSITY);
                unpolarized.emplace_back(0);
            }
        }
        lights_sets = {polarized, unpolarized, all};
    } else {
        lights_sets = {ae_conf.init_intensities};
    }
    params_next.lights = lights_sets.back();
    lights_sets.pop_back();

    /* ROI variables */
    if (ae_conf.roi.area() > 100){
        enable_roi = true;
        roi_weight = 1.0;
        roi = ae_conf.roi;
    } else {
        enable_roi = false;
        roi_weight = 1.0;
        roi = cv::Rect2d(0, 0, IMG_WIDTH, IMG_HEIGHT);
    }

    /* Algorithm variables */
    scale_et = scale_eg = 1;
    min_fit_gap = 100;
    thres_brt_diff = 15;
    thres_ue_brt = 5;
    thres_oe_brt = 250;
    et_intercept = 0;
    et_slope = 255.0/(MAX_ET - MIN_ET);

    /* initialize target brightness list */
    for (double i = 0.05; i <= 1.0;) {
        brt_list.emplace_back(static_cast<int>(255 * i));
        i += 0.1;
    }
    brt_target_ = brt_list.begin();
}

bool AEImpl::QuickTune(const cv::Mat &image, int brt_target, bool update_best) {
    /* Calc metrics in current frame */
    CalcMetrics(image);
    // printf("target brt = %d\n", brt_target);

    if (abs(status_cur.brt - brt_target) < thres_brt_diff || ++num_qt >= max_num_qt) {
        std::cout << "access target brt" << std::endl;
        num_qt = 0;
        if (update_best){
            UpdateBestParams();
        }
        return true;
    } else {
        CalcScaleFactors(brt_target);
        ScaleExpParams();
        return false;
    }
}

bool AEImpl::StepTune(const cv::Mat &image) {
    bool end_qt = QuickTune(image, *brt_target_, false);

    if (end_qt) {
        brt_target_++;
        num_qt = 0;
    } else if (oob_max) {
        while ((*brt_target_ > status_cur.brt) && (brt_target_ != brt_list.end())){
            brt_target_ ++;
            num_qt = 0;
        }
        oob_max = false;
    } else if (oob_min) {
        while ((*brt_target_ < status_cur.brt) && (brt_target_ != brt_list.end())){
            brt_target_ ++;
            num_qt = 0;
        }
        oob_min = false;
    }

    /* brt_target adjusted (end_qt or oob) */
    if (brt_target_ == brt_list.end()) {
        if (!lights_sets.empty()) {
            // update lights condition
            params_next.lights = lights_sets.back();
            lights_sets.pop_back();
            // reverse brt_list
            std::reverse(brt_list.begin(), brt_list.end());
            brt_target_ = brt_list.begin();
            status_prev.brt = status_cur.brt = -1;  // Note: reset brt status because of light change
            return false;
        } else {
            UpdateBestParams();
            return true;
        }
    } else {
        CalcScaleFactors(*brt_target_);
        ScaleExpParams();
        return false;
    }
}

void AEImpl::CalcMetrics(const cv::Mat &image) {
    /* Calc metrics */
    double brt =-1, entropy = -1, contrast = -1;
    auto start = std::chrono::high_resolution_clock::now();
    brt = CalcMeanBrt(image);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "CalcMeanBrt time: " << duration.count() / 1000 << std::endl;
    if (!enable_roi) {
        std::cout << "using entropy" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        entropy = CalcEntropy(image);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
        std::cout << "CalcEntropy time: " << duration.count() / 1000 << std::endl;
        ent_exp_map[entropy] = params_next;
    } else {
        std::cout << "using contrast" << std::endl;
        contrast = CalcContrast(image(roi));
        double score = roi_weight * 0.9 + contrast * 0.1;
        ct_exp_map[score] = params_next;
    }

    /* Update status */
    status_prev = status_cur;
    status_cur.params = params_next;
    status_cur.brt = brt;
    status_cur.entropy = entropy;
    status_cur.contrast = contrast;
    status_cur.Print();  // print status
    // printf("; roi = (%.2f, %.2f, %.2f, %.2f), hot_intensity = %.2f\n",
    //        roi.x, roi.y, roi.width, roi.height, roi_weight);
}

void AEImpl::UpdateBestParams() {
    if (!ct_exp_map.empty()) {
        /* exp params with max local contrast */
        params_best = ct_exp_map.rbegin()->second;
    } else {
        /* exp params with max global entropy */
        params_best = ent_exp_map.rbegin()->second;
    }
    params_next = params_best;
}

void AEImpl::CalcScaleFactors(int brt_target) {
    /* calc scale factors */
    double brt_cur = status_cur.brt;
    double scale_total = std::max(0.2, std::min(5.0, brt_target / brt_cur));

    /* assign scale factor based on MODE */
    double et_ori_scale, eg_ori_scale;
    if (MODE <= 0) {
        et_ori_scale = sqrt(scale_total);
        eg_ori_scale = sqrt(scale_total);
    } else if (MODE == 1) {
        et_ori_scale = scale_total;
        eg_ori_scale = 1.0;
    } else if (MODE == 2) {
        et_ori_scale = 1.0;
        eg_ori_scale = scale_total;
    }

    /* limit real ET scale range and calculate odd scale */
    double et_real_scale, et_odd_scale;

    /* linear regression of et and brt first */
    double max_et_scale, min_et_scale;
    LinearRegEtCurve(max_et_scale, min_et_scale);  // 只计算et的曲线

    if (et_ori_scale > max_et_scale) {
        std::cout << "et_ori_scale max range" << std::endl; 
        et_real_scale = max_et_scale;
        et_odd_scale = et_ori_scale / max_et_scale;
    } else if (et_ori_scale < min_et_scale) {
        std::cout << "et_ori_scale min range" << std::endl; 
        et_real_scale = min_et_scale;
        et_odd_scale = et_ori_scale / min_et_scale;
    } else {
        std::cout << "et_ori_scale middle range" << std::endl; 
        et_real_scale = et_ori_scale;
        et_odd_scale = 1.0;
    }

    /* limit real EG scale range and calculate odd scale */
    double max_eg_scale, min_eg_scale, eg_real_scale, eg_odd_scale;
    max_eg_scale = double(MAX_EG) / status_cur.params.exp_gain;
    min_eg_scale = double(MIN_EG) / status_cur.params.exp_gain;
    if (eg_ori_scale > max_eg_scale) {
        eg_real_scale = max_eg_scale;
        eg_odd_scale = eg_ori_scale / max_eg_scale;
    } else if (eg_ori_scale < min_eg_scale) {
        eg_real_scale = min_eg_scale;
        eg_odd_scale = eg_ori_scale / min_eg_scale;
    } else {
        eg_real_scale = eg_ori_scale;
        eg_odd_scale = 1.0;
    }

    /* cross multiply odd scale to real scale */
    scale_et = et_real_scale * eg_odd_scale;
    scale_eg = eg_real_scale * et_odd_scale;

    scale_et = std::max(scale_et, min_et_scale);
    scale_et = std::min(scale_et, max_et_scale);
    scale_eg = std::max(scale_eg, min_eg_scale);
    scale_eg = std::min(scale_eg, max_eg_scale);
}

void AEImpl::ScaleExpParams() {
    /* update next exposure params */
    int eg_cur = status_cur.params.exp_gain;
    double real_gain =  eg_cur * EG_QUANT_SCALE;
    double brt_cur = status_cur.brt;
    double brt_cur_et = brt_cur / real_gain;  // 除以eg后，得到的就是由et提供的亮度

    params_next.exp_time = static_cast<int>(((brt_cur_et * scale_et) - et_intercept) / et_slope);
    params_next.exp_gain = static_cast<int>(round(eg_cur * scale_eg));

    /* boundary check */
    bool et_rmax = (params_next.exp_time >= MAX_ET && status_cur.params.exp_time == MAX_ET);
    bool eg_rmax = (params_next.exp_gain >= MAX_EG && status_cur.params.exp_gain == MAX_EG);
    bool et_rmin = (params_next.exp_time <= MIN_ET && status_cur.params.exp_time == MIN_ET);
    bool eg_rmin = (params_next.exp_gain <= MIN_EG && status_cur.params.exp_gain == MIN_EG);
    if (et_rmax && eg_rmax) {
        oob_max = true;
        printf("ET and EG reach MAX boundary\n");
    } else if (et_rmin && eg_rmin) {
        oob_min = true;
        printf("ET and EG reach MIN boundary\n");
    }

    /* limit next params in boundary */
    params_next.exp_time = std::min(MAX_ET, params_next.exp_time);
    params_next.exp_time = std::max(MIN_ET, params_next.exp_time);
    params_next.exp_gain = std::min(MAX_EG, params_next.exp_gain);
    params_next.exp_gain = std::max(MIN_EG, params_next.exp_gain);
}

double AEImpl::CalcMeanBrt(const cv::Mat &image) {
    /* Calc image mean brightness */
    double mean_brt = cv::mean(image)[0];
    return mean_brt;
}

double AEImpl::CalcEntropy(const cv::Mat &image) {
    /* Calc histogram and probability */
    cv::Mat hist;
    int channels[] = {0};
    int size[] = {256};
    float range[] = {0, 256};
    const float *ranges[] = {range};
    cv::calcHist(&image, 1, channels, cv::Mat(), hist, 1, size, ranges, true, false);
    cv::Mat prob = hist / (image.rows * image.cols);

    /* apply equation to calc entropy */
    double entropy = 0;
    for (int i = 0; i < 256; i++) {
        if (prob.at<float>(i) > 0.0)
            entropy -= prob.at<float>(i) * log2(prob.at<float>(i));
    }

    return entropy;
}

double AEImpl::CalcContrast(const cv::Mat &image) {
    /* Binary image by OTSU */
    cv::Mat image_bin;
    double thresh = cv::threshold(image, image_bin, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    /* Get min and max of the image */
    double img_min, img_max;
    cv::minMaxLoc(image, &img_min, &img_max);
    if (img_min == img_max) {
        return 0;
    }

    /* split histogram into low and high brightness */
    double low_hist[256] = {0};
    double high_hist[256] = {0};
    for (int i = 0; i < image.rows; ++i) {
        for (int j = 0; j < image.cols; ++j) {
            auto pixel = image.ptr<uchar>(i)[j];
            if (pixel < thresh) {
                low_hist[pixel]++;
            } else {
                high_hist[pixel]++;
            }
        }
    }

    /* calc mean brightness of low and high brightness and contrast */
    double low_brt = 0, high_brt = 0;
    double low_nums = 0, high_nums = 0;
    for (int i = 0; i < 256; ++i) {
        low_brt += i * low_hist[i];
        high_brt += i * high_hist[i];
        low_nums += low_hist[i];
        high_nums += high_hist[i];
    }
    double low_brt_mean = low_brt / (low_nums + 1e-9);
    double high_brt_mean = high_brt / (high_nums + 1e-9);

//    /* calc variance of low and high brightness */
//    double low_var = 0, high_var = 0;
//    for (int i = 0; i < 256; ++i) {
//        low_var += pow((i - low_brt_mean)/256, 2) * low_hist[i];
//        high_var += pow((i - high_brt_mean)/256, 2) * high_hist[i];
//    }
//    double var_score = (1 + exp(-1 * (low_var + high_var))) / 2;

    double contrast = (high_brt_mean - low_brt_mean);

    return contrast;
}

void AEImpl::LinearRegEtCurve(double &max_et_scale, double &min_et_scale) {
    /* info of current and previous status */
    double et_cur = status_cur.params.exp_time;
    double eg_cur = status_cur.params.exp_gain * EG_QUANT_SCALE;
    double brt_cur_et = status_cur.brt / eg_cur;     // 除以eg后，得到的就是由et提供的亮度

    double et_prev = status_prev.params.exp_time;
    double eg_prev = status_prev.params.exp_gain * EG_QUANT_SCALE;
    double brt_prev_et = status_prev.brt / eg_prev;  // 除以eg后，得到的就是由et提供的亮度

    /* Check if meet the condition of linear regression */
    bool not_oue = (status_prev.brt >= thres_ue_brt && status_prev.brt<= thres_oe_brt) &&    //  5 <= status_prev.brt <= 250
                   (status_cur.brt >= thres_ue_brt && status_cur.brt<= thres_oe_brt);        //  5 <= status_cur.brt <= 250
    bool mono_inc = (brt_cur_et - brt_prev_et) * (et_cur - et_prev) > 0;
    bool meet_gap = abs(et_cur - et_prev) >= min_fit_gap;

    // et为横轴，brt为纵轴的一次函数

    /* Fit line function of et and brt by two points */
    if (meet_gap && not_oue && mono_inc) {
        et_slope = (brt_cur_et - brt_prev_et) / (et_cur - et_prev);   // k = (y2-y1) / (x2-x1)
        et_intercept = brt_cur_et - et_slope * et_cur;                // b = y2 - k*x2
    } else {
        et_slope = (brt_cur_et) / double(et_cur);        // 截距为0时，k = y / x
        et_intercept = 0;                             
    }

    std::cout << "et_slope " << et_slope << "  et_intercept " << et_intercept << std::endl;

    /* calc max and min et scale */
    double max_brt = std::min((MAX_ET * et_slope + et_intercept), 255.0);
    double min_brt = std::max((MIN_ET * et_slope + et_intercept), 0.0);

    max_et_scale = max_brt / (brt_cur_et);
    min_et_scale = min_brt / (brt_cur_et);
}
