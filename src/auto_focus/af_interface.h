#ifndef AF_INTERFACE_H
#define AF_INTERFACE_H

#include "af_impl.h"

namespace af {
    class AFInterface {
    public:
        bool end_iter;                      // flag to indicate whether the AF algorithm is done
        bool enable_hmap;                   // flag to indicate whether the heat map is used
        int next_pos;                       // the next position of the lens
        int best_pos;                       // the best position of the lens
        std::vector<HotspotInfo> id_hots;   // the information of the hotspots
        HotspotInfo dummy_hotspot;          // the dummy hotspot

        bool start_fit = false;

    public:
        /** @brief The default constructor. */
        AFInterface();

        /** @brief The default destructor. */
        ~AFInterface() = default;

        /** @brief Init the AF algorithm
         *  @param start_pos the start position of the lens
         *  @param end_pos the end position of the lens
         *  @param image_size the size of the input image
         *  @param enable_hmap flag to indicate whether the heat map is used
         */
        void Init(AFConf &af_conf, bool enable_hmap);

        /** @brief Run the AF algorithm
         *  @param img the input image
         *  @param hmap the heat map of the input image
         */
        void Run(const cv::Mat &img, const std::vector<cv::Rect> &rois={});

    private:
        /* the implementation object of the AF algorithm */
        std::shared_ptr<AFImpl> af_impl_;

    private:
        void ExtractHotspots(const cv::Mat &hmap, double intensity_thresh = 100);

        void FusionHotspots(std::vector<HotspotInfo> &new_hots, double iou_thresh = 0.2);

        static double CalcHotsIOU(const HotspotInfo &hot1, const HotspotInfo &hot2);
    };
}

#endif //AF_INTERFACE_H
