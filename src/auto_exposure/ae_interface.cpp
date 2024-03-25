#include "ae_interface.h"

using namespace ae;

AEInterface::AEInterface() {
    this->end_qt = false;
    this->end_st = false;
    this->enable_hmap = false;
}

void AEInterface::Init(AEConf &ae_cam_conf, bool en_al, bool en_hmap) {
    this->enable_hmap = en_hmap;
    this->end_qt = false;
    this->end_st = false;
    this->img_width_ = ae_cam_conf.img_width;
    this->img_height_ = ae_cam_conf.img_height;

    ae_impl_ = std::make_shared<AEImpl>(ae_cam_conf, en_al);
    params_next = ae_impl_->params_next;
}

void AEInterface::QuickTune(const cv::Mat &image, int brt_target) {
    /* Quick tuning */
    end_qt = ae_impl_->QuickTune(image, brt_target);
    params_next = ae_impl_->params_next;
    if (end_qt) {
        params_best = ae_impl_->params_best;
    }
}

void AEInterface::StepTune(const cv::Mat &image, const cv::Mat &hmap) {
    /* Extract the hotspot from the heatmap if the heatmap is not empty */
    if (enable_hmap && !hmap.empty()) {
        cv::Rect2d roi;
        double intensity;
        LocateMaxHotspot(hmap, roi, intensity);

        if (intensity > 100) {
            ae_impl_->enable_roi = true;
            ae_impl_->roi = ExpandRect(roi);
            ae_impl_->roi_weight = intensity / 255.0;
        } else {
            ae_impl_->enable_roi = false;
            ae_impl_->roi = cv::Rect2d(0, 0, 0, 0);
            ae_impl_->roi_weight = 1.0;
        }
    } else {
        ae_impl_->enable_roi = false;
    }

    /* Step tuning */
    end_st = ae_impl_->StepTune(image);
    params_next = ae_impl_->params_next;
    if (end_st) {
        params_best = ae_impl_->params_best;
    }
}

void AEInterface::LocateMaxHotspot(const cv::Mat &hmap, cv::Rect2d &roi, double &intensity, double hot_thresh) {
    roi = cv::Rect2d(0, 0, 0, 0);
    intensity = 0;
    double max_sum_intensity = 0;

    std::vector<cv::Mat> hmap_chs;
    cv::split(hmap, hmap_chs);
    for (const auto &hmap_ch: hmap_chs) {
        /* Filter out the channel with low max intensity */
        double max_ch_intensity;
        cv::minMaxLoc(hmap_ch, nullptr, &max_ch_intensity);
        if (max_ch_intensity < hot_thresh) {
            continue;
        }

        /* Threshold the heatmap channel to get the binary hmap, and filter out the channel with low threshold */
        cv::Mat hmap_bin;
        double thresh = cv::threshold(hmap_ch, hmap_bin, 0, 1, cv::THRESH_BINARY | cv::THRESH_OTSU);
        if ((thresh < hot_thresh / 2) && (thresh > 1e-9)) {
            continue;
        }

        /* Find the connected components in the binary hmap */
        cv::Mat labels, stats, centroids;
        cv::connectedComponentsWithStats(hmap_bin, labels, stats, centroids);

        /* Get ROI from the connected components and calculate the mean intensity of each ROI */
        for (int i = 1; i < stats.rows; i++) {
            // filter out the connected component with small area
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area < 100) {  // TODO: make thresholds configurable
                continue;
            }

            // get the roi of the connected component and calculate the mean intensity
            cv::Rect2d rect(stats.at<int>(i, cv::CC_STAT_LEFT),
                            stats.at<int>(i, cv::CC_STAT_TOP),
                            stats.at<int>(i, cv::CC_STAT_WIDTH),
                            stats.at<int>(i, cv::CC_STAT_HEIGHT));
            double sum_intensity = cv::sum(hmap_ch(rect))[0];
            double mean_intensity = sum_intensity / area;

            // update the max roi and intensity
            if (sum_intensity > max_sum_intensity) {
                roi = rect;
                intensity = mean_intensity;
                max_sum_intensity = sum_intensity;
            }
        }
    }
}

cv::Rect2d AEInterface::ExpandRect(const cv::Rect2d &rect) {
    cv::Rect2d expand_rect;
    expand_rect.x = rect.x - rect.width * (2 - 1) / 2;
    expand_rect.y = rect.y - rect.height * (2 - 1) / 2;
    expand_rect.width = rect.width * 2;
    expand_rect.height = rect.height * 2;

    // boundary check
    if (expand_rect.x < 0) {
        expand_rect.width += expand_rect.x;
        expand_rect.x = 0;
    }
    if (expand_rect.y < 0) {
        expand_rect.height += expand_rect.y;
        expand_rect.y = 0;
    }
    if (expand_rect.x + expand_rect.width >= img_width_) {
        expand_rect.width = img_width_ - 1 - expand_rect.x;
    }
    if (expand_rect.y + expand_rect.height >= img_height_) {
        expand_rect.height = img_height_ - 1 - expand_rect.y;
    }
    return expand_rect;
}