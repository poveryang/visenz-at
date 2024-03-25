#include "hmap_infer_base.h"

std::vector<Hotspot> HMapInferBase::LocateHotspots(const cv::Mat &heatmap, double intensity_thres) {
    /* Split the heatmap into channels and iterate over each channel */
    std::vector<Hotspot> hotspots;
    std::vector<cv::Mat> heatmap_chs;
    cv::split(heatmap, heatmap_chs);
    for (int ch = 0; ch < heatmap_chs.size(); ch++) {
        cv::Mat heatmap_ch = heatmap_chs[ch];

        /* Filter out the channel with low max intensity */
        double max_ch_intensity;
        cv::minMaxLoc(heatmap_ch, nullptr, &max_ch_intensity);
        if (max_ch_intensity < intensity_thres) {
            continue;
        }

        /* Threshold the heatmap channel to get the binary hmap, and filter out the channel with low threshold */
        cv::Mat hmap_bin;
        double thresh = cv::threshold(heatmap_ch, hmap_bin, 0, 1, cv::THRESH_BINARY | cv::THRESH_OTSU);
        if ((thresh < intensity_thres / 2) && (thresh > 1e-9)) {
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

            cv::Point2d centroid = cv::Point2d(centroids.at<double>(i, 0), centroids.at<double>(i, 1));
            cv::Rect2d rect(stats.at<int>(i, cv::CC_STAT_LEFT),
                            stats.at<int>(i, cv::CC_STAT_TOP),
                            stats.at<int>(i, cv::CC_STAT_WIDTH),
                            stats.at<int>(i, cv::CC_STAT_HEIGHT));
            double sum_intensity = cv::sum(heatmap_ch(rect))[0];
            double mean_intensity = sum_intensity / area;

            // store the hmap info
            Hotspot hotspot;
            hotspot.type = ch;
            hotspot.area = area;
            hotspot.centroid = centroid;
            hotspot.rect = rect;
            hotspot.sum_intensity = sum_intensity;
            hotspot.mean_intensity = mean_intensity;
            hotspot.mask = hmap_bin(rect);
            IdentifyHotspot(hotspot);
            hotspots.emplace_back(hotspot);
        }
    }

    return hotspots;
}

Hotspot HMapInferBase::LocateMaxHotspot(const cv::Mat &heatmap, double intensity_thres) {
    // create a new hotspot without any info
    Hotspot max_hotspot;

    /* Locate all the hotspots */
    std::vector<Hotspot> hotspots = LocateHotspots(heatmap, intensity_thres);

    /* Pick the hotspot with the highest intensity */
    for (const auto &hots: hotspots) {
        if (hots.sum_intensity > max_hotspot.sum_intensity) {
            max_hotspot = hots;
        }
    }
    return max_hotspot;
}

void HMapInferBase::IdentifyHotspot(Hotspot &new_hotspot) {
    /* Match the new hotspot with the existing hotspots */
    for (auto &exist_hotspot: hots_recoder) {
        // Determine if the new hotspot is the same as the existing hotspot
        if (new_hotspot.type != exist_hotspot.type) {
            continue;
        }

        double iou = CalcHotsIOU(new_hotspot, exist_hotspot);
        if (iou > 0.2) {  // TODO: make this threshold configurable
            new_hotspot.id = exist_hotspot.id;
            exist_hotspot.sum_intensity = std::max(new_hotspot.sum_intensity, exist_hotspot.sum_intensity);
            return;
        }
    }

    /* If the hotspot is not matched with any existing hotspot, assign a new id */
    new_hotspot.id = static_cast<int>(hots_recoder.size());
    hots_recoder.emplace_back(new_hotspot);
}

double HMapInferBase::CalcHotsIOU(const Hotspot &hot1, const Hotspot &hot2) {
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