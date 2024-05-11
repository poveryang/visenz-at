/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AE_INTERFACE_H
#define AE_INTERFACE_H

#include "ae_impl.h"


namespace ae {
    class AEInterface {
    public:
        AEInterface();

        /**
         * Initialization of Auto Exposure Module
         * @param mode execution mode, 0:auto, 1: shutter priority, 2: gain priority
         * @param MIN_ET minimum exposure time
         * @param max_et maximum exposure time
         * @param min_eg minimum exposure gain
         * @param max_eg maximum exposure gain
         */
        void Init(AEConf &ae_conf, bool en_al, bool en_hmap);

        /**
         * Tuning exposure parameters to close to the target brightness
         * @param image input image
         * @param brt_target target brightness
         */
        void QuickTune(const cv::Mat &image, int brt_target=128, const std::vector<cv::Rect> &rois={});

        /**
         * Tuning exposure parameters stepwise
         * @param image input image
         * @param hmap heatmap, default is empty
         */
        void StepTune(const cv::Mat &image, const cv::Mat &hmap = cv::Mat());

        void UpdateLights();

        /** The default destructor. */
        ~AEInterface() = default;

        std::shared_ptr<AEImpl> ae_impl_;
    public:
        bool enable_hmap;
        bool end_qt;
        bool end_st;
        bool ae_fail;
        AEParams params_next, params_best;

    private:
        int img_width_, img_height_;
        static void LocateMaxHotspot(const cv::Mat &hmap, cv::Rect2d &roi, double &intensity, double hot_thresh = 200);

        cv::Rect2d ExpandRect(const cv::Rect2d &roi);
    };
}  // namespace ae

#endif //AE_INTERFACE_H
