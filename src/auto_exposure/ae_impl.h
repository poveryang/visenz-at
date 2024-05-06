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

        /** Resolution and ROI*/
        int img_width;                      // The width of the image
        int img_height;                     // The height of the image
        cv::Rect2d roi;                     // roi of the image

        /** Initial exposure params*/
        int init_et;                        // initial exposure time
        int init_eg;                        // initial exposure gain
        std::vector<int> init_intensities;  // initial light intensities
    };

    /* Exposure params (include fill-light) */
    struct AEParams {
        int exp_time;
        int exp_gain;
        std::vector<int> lights;

        void Print() const {
            printf("Exp time=%d, Exp gain=%d, Lights=(", exp_time, exp_gain);
            for (auto &light : lights) {
                printf("%d,", light);
            }
            printf("); ");
        }
    };

    /* Status of AE */
    struct AEStatus {
        double brt;
        double entropy;
        double contrast;
        AEParams params;

        void Print() const {
            params.Print();
            printf("brt=%.2f, entropy=%.2f, contrast=%.2f; \n", brt, entropy, contrast);
        }
    };

    /* Implementation of Auto Exposure Module */
    class AEImpl {
    public:
        /* Camera config related to AE */
        int MODE;
        int MIN_ET, MAX_ET;
        int MIN_EG, MAX_EG;
        double EG_QUANT_SCALE;              // quantization scale of exposure gain
        int MAX_LT, MIN_INTENSITY, MAX_INTENSITY;
        int IMG_WIDTH, IMG_HEIGHT;

        int init_eg;

        /* Basic variables */
        bool oob_max;                               // out of max bound
        bool oob_min;                               // out of min bound
        int num_qt, max_num_qt;                     // number of quick tuning
        AEStatus status_prev, status_cur;
        AEParams params_next, params_best;

        /* ROI variables */
        bool enable_roi;
        double roi_weight;
        cv::Rect2d roi;

        /* Algorithm variables */
        double scale_et, scale_eg;
        double thres_brt_diff, thres_ue_brt, thres_oe_brt;
        double et_slope, et_intercept;
        int min_fit_gap;

        int tuning_count;
        int max_tuning_count;
        bool ae_fail;

        /* Metrics values with corresponding exposure params */
        std::vector<std::vector<int>> lights_sets;   // sets of lights
        std::vector<int> brt_list;                   // list of target brightness
        std::vector<int>::iterator brt_target_;      // target brightness

        std::map<double, AEParams> ent_exp_map;      // map of entropy and exposure params
        std::map<double, AEParams> ct_exp_map;       // map of contrast and exposure params
        std::map<double, AEParams> brt_exp_map;      // map of brightness and exposure params

    public:
        explicit AEImpl(const AEConf &ae_cam_conf, bool en_al);

        ~AEImpl() = default;

        bool StepTune(const cv::Mat &image);

        bool QuickTune(const cv::Mat &image, int brt_target=128, bool update_best=true, const std::vector<cv::Rect> &rois={});

        void UpdateBestParams();

        void CalcMetrics(const cv::Mat &image, const std::vector<cv::Rect> &rois);

        void ScaleExpParams();

        static double CalcMeanBrt(const cv::Mat &image, const std::vector<cv::Rect> &rois);

        static double CalcEntropy(const cv::Mat &image);

        static double CalcContrast(const cv::Mat &image) ;

        void LinearRegEtCurve(double &max_et_scale, double &min_et_scale);

        void CalcScaleFactors(int brt_target);
    };
} // ae

#endif //AE_ST_IMPL_H
