/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AE_ST_IMPL_H
#define AE_ST_IMPL_H

#include <opencv2/opencv.hpp>

namespace ae {
    /* Camera config related to AE */
    struct AEConf {
        /** Exposure control*/
        int ae_mode;                        // 0:auto, 1: shutter priority, 2: gain priority
        int min_et;                         // minimum exposure time
        int max_et;                         // maximum exposure time
        int min_eg;                         // minimum exposure gain
        int max_eg;                         // maximum exposure gain
        double eg_quant_scale;              // quantization scale of exposure gain

        /** Fill Light control*/
        int max_lt;                         // maximum light time
        int min_intensity;                  // minimum light intensity
        int max_intensity;                  // maximum light intensity

        /** Initial exposure params*/
        int init_et;                        // initial exposure time
        int init_eg;                        // initial exposure gain
        std::vector<int> init_intensities;  // initial light intensities
    };

    /* Exposure params (include fill-light) */
    struct AEParams 
    {
        int exp_time;
        int exp_gain;
        std::vector<int> lights;

        void Print() const 
        {
            printf("Exp time=%d, Exp gain=%d, Lights=(", exp_time, exp_gain);
            for (auto &light : lights) 
            {
                printf("%d,", light);
            }
            printf("); ");
        }
    };

    /* Status of AE */
    struct AEStatus 
    {
        double brt;
        // double entropy;
        // double contrast;
        AEParams params;

        void Print() const 
        {
            params.Print();
            // printf("brt=%.2f, entropy=%.2f, contrast=%.2f; \n", brt, entropy, contrast);
            printf("brt=%.2f; \n", brt);
        }
    };

    /* Implementation of Auto Exposure Module */
    class AEImpl 
    {
    public:
        /* Camera config related to AE */
        int mode;
        int min_et, max_et;
        int min_eg, max_eg;
        int min_intensity, max_intensity;

        int init_eg;
        AEStatus status_cur;
        AEParams params_next, params_best;

        int tuning_count;
        int max_tuning_count;
        bool ae_fail;

        const int MAX_ET_LIMIT = 40000;  // max_et是从相机读回来的，但是at过程中，不需要去到相机的上限，需要给定一个et的最大值为40ms

        /* Metrics values with corresponding exposure params */
        std::vector<std::vector<int>> lights_sets;   // sets of lights

    public:
        explicit AEImpl(const AEConf &ae_cam_conf, bool en_al);

        ~AEImpl() = default;

        bool QuickTune(const cv::Mat &image, int brt_target=128, const std::vector<cv::Rect> &rois={}, int brt_diff_thre=15, bool enable_switch=true);

        void CalcMetrics(const cv::Mat &image, const std::vector<cv::Rect> &rois);

        static double CalcMeanBrt(const cv::Mat &image, const std::vector<cv::Rect> &rois);

        bool UpdateLights();

        void UpdateExposureAndGain(int brt_target);
    };
} // namespace ae

#endif //AE_ST_IMPL_H
