#include "af_impl.h"
#include "af_config.h"

using namespace af;

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
    this->roi = cv::Rect2d(0, 0, 0, 0);

    /* misc */
    this->win_size = af_alg_conf.win_size;
    this->img_width = af_conf.IMG_WIDTH;
    this->img_height = af_conf.IMG_HEIGHT;
    this->pos_samples.clear();
    this->pos_with_fv.clear();
    this->pos_fv_recorder.clear();
    this->id_hots.clear();

    /* init the sampler and get the first sample */
    ResetSamples();
    GetNextSample();
}

void AFImpl::Run(const cv::Mat &image) {
    CalcFocusValue(image);

    if (start_fit) {
        PolynomialFit();
    } else if (enable_hmap) {
        AnalysisHotspots();
    } else {
        AnalysisFvCurve();
    }

    GetNextSample();
}

void AFImpl::CalcFocusValue(const cv::Mat &image) {
    cv::Mat image_clone = image.clone();
    if (enable_roi) {
        image_clone = image_clone(roi);
    }

    // resize the image if the image is too large
    if (image_clone.cols > 1280 || image_clone.rows > 800) {
        double scale = std::min(1280.0 / image_clone.cols, 800.0 / image_clone.rows);
        cv::resize(image_clone, image_clone, cv::Size(), scale, scale);
    }

    cv::Mat x_grad, y_grad;
    cv::Sobel(image_clone, x_grad, CV_32FC1, 1, 0, 11);
    cv::Sobel(image_clone, y_grad, CV_32FC1, 0, 1, 11);

    double acc = 0;
    int cols = image_clone.cols;
    int rows = image_clone.rows;
#pragma omp parallel for reduction(+ : acc) default(none) shared(x_grad, y_grad, rows, cols)
    for (int row = 1; row < rows - 1; ++row) {
        auto ptr_x = x_grad.ptr<float>(row);
        auto ptr_y = y_grad.ptr<float>(row);
        for (int col = 1; col < cols - 1; ++col) {
            acc += ptr_x[col] * ptr_x[col];
            acc += ptr_y[col] * ptr_y[col];
        }
    }

    pos_with_fv[next_pos] = acc;
    pos_fv_recorder[next_pos] = acc;
    printf("Focus pos=%d, focus value=%.2e\n", next_pos, acc);
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
        UpdateStatus(pos_vec, peak_idx);
        printf("Apply max fv in pos %d\n", pos_cur_center);
    } else {
        /* Situation 2: Sampling in progress: search for peak in the focus value curve */
        std::vector<int> pos_vec;
        std::vector<double> fv_vec;
        GetPosFv(pos_vec, fv_vec);
        int peak_idx = SlidingWinSearch(fv_vec, win_size, false);
        if (peak_idx != -1) {
            UpdateStatus(pos_vec, peak_idx);
            printf("Found peak fv in pos %d\n", pos_cur_center);
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
//        printf("Fitting sampling: start = %d, end = %d, step = %d\n", pos_cur_center-6, pos_cur_center+6, 2);
        pos_samples = sampler.StepSampling(pos_cur_center - 6, pos_cur_center + 6, 2);
    } else if (enable_hmap) {
//        printf("HMap sampling: start = %d, end = %d, step = %d\n", pos_start, pos_end, hmap_pos_step);
        pos_samples = sampler.StepSampling(pos_start, pos_end, hmap_pos_step);
    } else {
//        printf("Center sampling: start = %d, end = %d, center = %d\n", pos_start, pos_end, pos_cur_center);
        pos_samples = sampler.SparseSampling(pos_start, pos_end, pos_cur_center);
    }
    win_size = std::max(3, static_cast<int>(pos_samples.size() / 2));

    /* If pos has been sampled before, add it to the pos_with_fv and remove it from the pos_samples*/
    for (auto it = pos_samples.begin(); it != pos_samples.end();) {
        if (pos_fv_recorder.find(*it) != pos_fv_recorder.end()) {
            pos_with_fv[*it] = pos_fv_recorder[*it];
            it = pos_samples.erase(it);
            win_size += 1;
        } else {
            it++;
        }
    }
}

void AFImpl::GetNextSample() {
    if (!pos_samples.empty()) {
        next_pos = pos_samples.front();
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
        if (is_peak) {
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
    for (auto &pos_fv: pos_with_fv) {
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
