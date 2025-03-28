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
        void Init(AEConf &ae_conf, bool en_al);

        /**
         * Tuning exposure parameters to close to the target brightness
         * @param image input image
         * @param brt_target target brightness
         */
        void QuickTune(const cv::Mat &image, int brt_target=128, const std::vector<cv::Rect> &rois={}, int brt_diff_thre=15, bool enable_switch=true);

        bool UpdateLights();

        void ClearState();

        void ResetTunningCount();

        void SetParam(int exp_time, int exp_gain, std::vector<int> lights);

        /** The default destructor. */
        ~AEInterface() = default;

        std::shared_ptr<AEImpl> ae_impl_;
    public:
        bool end_qt;
        bool ae_fail;
        bool exceed_tunning_count;
        AEParams params_next, params_best;
    };
}  // namespace ae

#endif //AE_INTERFACE_H
