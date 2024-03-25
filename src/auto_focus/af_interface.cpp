#include "af_interface.h"


using namespace af;

AFInterface::AFInterface() {
    end_iter = enable_hmap = false;
    next_pos = best_pos = -1;
    id_hots.clear();
    dummy_hotspot.positions.clear();
    dummy_hotspot.intensities.clear();
}

void AFInterface::Init(AFConf &af_conf, bool en_hmap) {
    enable_hmap = en_hmap;
    af_impl_ = std::make_shared<AFImpl>(af_conf, en_hmap);
    next_pos = af_impl_->next_pos;
}

void AFInterface::Run(const cv::Mat &img, const cv::Mat &hmap) {
    /* Extract the hotspots from the heat map */
    if (enable_hmap && !hmap.empty()) {
//        cv::imwrite("/tmp/at_res/hmap_pos_" + std::to_string(next_pos) + ".png", hmap);
        ExtractHotspots(hmap);
        af_impl_->id_hots = id_hots;
    }
    /* Run the AF algorithm and update variables */
    af_impl_->Run(img);

    next_pos = af_impl_->next_pos;
    enable_hmap = af_impl_->enable_hmap;
    end_iter = af_impl_->end_iter;
    if (end_iter) {
        best_pos = af_impl_->best_pos;
        id_hots.clear();
        dummy_hotspot.positions.clear();
        dummy_hotspot.intensities.clear();
    }
}

void AFInterface::ExtractHotspots(const cv::Mat &hmap, double intensity_thresh) {
    /* Split the heatmap into channels and iterate over each channel */
    std::vector<HotspotInfo> new_hots;
    std::vector<cv::Mat> hmap_chs;
    cv::split(hmap, hmap_chs);
    for (int ch = 0; ch < hmap_chs.size(); ch++) {
        cv::Mat heatmap_ch = hmap_chs[ch];

        /* Filter out the channel with low max intensity */
        double max_ch_intensity;
        cv::minMaxLoc(heatmap_ch, nullptr, &max_ch_intensity);
        if (max_ch_intensity < intensity_thresh) {
            continue;
        }

        /* Threshold the heatmap channel to get the binary hmap, and filter out the channel with low threshold */
        cv::Mat hmap_bin;
        double thresh = cv::threshold(heatmap_ch, hmap_bin, 0, 1, cv::THRESH_BINARY | cv::THRESH_OTSU);
        if ((thresh < intensity_thresh / 2) && (thresh > 1e-9)) {
            continue;
        }

        /* Find the connected components in the binary hmap */
        cv::Mat labels, stats, centroids;
        cv::connectedComponentsWithStats(hmap_bin, labels, stats, centroids);

        /* Get ROI from the connected components and calculate the mean intensity of each ROI */
        for (int i = 1; i < stats.rows; i++) {
            // filter out the connected component with small area
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area < 600) {  // TODO: make thresholds configurable
                continue;
            }

            cv::Point2d centroid = cv::Point2d(centroids.at<double>(i, 0), centroids.at<double>(i, 1));
            cv::Rect2d rect(stats.at<int>(i, cv::CC_STAT_LEFT),
                            stats.at<int>(i, cv::CC_STAT_TOP),
                            stats.at<int>(i, cv::CC_STAT_WIDTH),
                            stats.at<int>(i, cv::CC_STAT_HEIGHT));
            double sum_intensity = cv::sum(heatmap_ch(rect))[0];
            double mean_intensity = sum_intensity / area;
            if (mean_intensity < intensity_thresh) {
                continue;
            }

            // store the hmap info
            HotspotInfo new_hot;
            new_hot.id = -1;  // indicate that it is a new hotspot
            new_hot.type = ch;
            new_hot.area = area;
            new_hot.mask = hmap_bin(rect);
            new_hot.rect = rect;
            new_hot.max_intensity = sum_intensity;
            new_hots.emplace_back(new_hot);
            printf("Hotspot: type %d, area %d, mean intensity %f\n", ch, area, mean_intensity);
        }
    }

    /* Fusion the new hotspots with the existed hotspots */
    FusionHotspots(new_hots);
}

void AFInterface::FusionHotspots(std::vector<HotspotInfo> &new_hots, double iou_thresh) {
    /* Update the dummy hotspot to record positions without intensity */
    dummy_hotspot.positions.emplace_back(next_pos);
    dummy_hotspot.intensities.emplace_back(0);

    /* Identify the new hotspots by comparing with the existed hotspots */
    for (auto &old_hot: id_hots) {
        old_hot.positions.emplace_back(next_pos);
        old_hot.intensities.emplace_back(0);

        for (auto &new_hot: new_hots) {
            if (new_hot.type == old_hot.type && new_hot.id == -1) {
                double iou = CalcHotsIOU(new_hot, old_hot);
                if (iou > iou_thresh) {
                    new_hot.id = old_hot.id;
                    // update the existed hotspot
                    old_hot.intensities.back() = new_hot.max_intensity;
                    if (new_hot.max_intensity > old_hot.max_intensity) {
                        old_hot.area = new_hot.area;
                        old_hot.mask = new_hot.mask;
                        old_hot.rect = new_hot.rect;
                        old_hot.max_intensity = new_hot.max_intensity;
                    }
                    break;
                }
            }
        }
    }

    for (auto &new_hot: new_hots) {
        // store the new hotspots
        if (new_hot.id == -1) {
            new_hot.id = static_cast<int>(id_hots.size());
            new_hot.positions = dummy_hotspot.positions;
            new_hot.intensities = dummy_hotspot.intensities;
            new_hot.intensities.back() = new_hot.max_intensity;  // inplace update the intensity of the new hotspot
            id_hots.emplace_back(new_hot);
        }
    }

    /* Sort the hotspots by max intensity */
    std::sort(id_hots.begin(),
              id_hots.end(),
              [](const HotspotInfo &hot1, const HotspotInfo &hot2) {
                  return hot1.max_intensity > hot2.max_intensity;
              });
}

double AFInterface::CalcHotsIOU(const HotspotInfo &hot1, const HotspotInfo &hot2) {
    // rects coordination
    cv::Rect2d rect1 = hot1.rect;
    cv::Rect2d rect2 = hot2.rect;
    double x11 = rect1.x;
    double y11 = rect1.y;
    double x12 = rect1.x + rect1.width;
    double y12 = rect1.y + rect1.height;
    double x21 = rect2.x;
    double y21 = rect2.y;
    double x22 = rect2.x + rect2.width;
    double y22 = rect2.y + rect2.height;

    // intersection rect
    cv::Rect2d int_rect1;
    cv::Rect2d int_rect2;
    double x = std::max(x11, x21);
    double y = std::max(y11, y21);
    double width = std::min(x12, x22) - x;
    double height = std::min(y12, y22) - y;
    cv::Rect2d int_rect = cv::Rect2d(x, y, width, height);
    int_rect1 = int_rect - cv::Point2d(x11, y11);
    int_rect2 = int_rect - cv::Point2d(x21, y21);
    if (int_rect1.width <= 0 || int_rect1.height <= 0 || int_rect2.width <= 0 || int_rect2.height <= 0) {
        return 0;
    }

    // intersection area
    cv::Mat mask1 = hot1.mask;
    cv::Mat mask2 = hot2.mask;
    cv::Mat mask_int = mask1(int_rect1) & mask2(int_rect2);

    double area1 = hot1.area;
    double area2 = hot2.area;
    double area_int = cv::sum(mask_int)[0];
    double iou = area_int / (area1 + area2 - area_int);
    return iou;
}
