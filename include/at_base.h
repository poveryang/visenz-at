#ifndef AT_PARAMS_H
#define AT_PARAMS_H

#include "ar_base.h"
#include <opencv2/opencv.hpp>

/** @brief Camera configuration.*/
struct CamConf {
    /** Image Resolution*/
    int img_width;                      // image width
    int img_height;                     // image height
    cv::Rect2d roi;                     // roi of the image

    /** Fill Light control*/
    int max_lt;                         // maximum light time
    int min_intensity;                  // minimum light intensity
    int max_intensity;                  // maximum light intensity

    /** Exposure control*/
    int min_et;                         // minimum exposure time
    int max_et;                         // maximum exposure time
    int min_eg;                         // minimum exposure gain
    int max_eg;                         // maximum exposure gain
    double eg_quant_scale;              // quantization scale of exposure gain

    /** Focus control*/
    int lens_type;                      // 0: mechanical lens, 1: liquid lens
    int start_pos;                      // start position of lens
    int end_pos;                        // end position of lens

    /** Initial camera parameters*/
    int ae_mode;                        // 0:auto, 1: shutter priority, 2: gain priority
    int init_et;                        // initial exposure time
    int init_eg;                        // initial exposure gain
    std::vector<int> init_intensities;  // initial light intensities
    int init_pos;                       // initial position of lens
};

/** @brief Camera parameters.*/
struct CamParams {
    int exp_time; // exposure time
    int exp_gain; // exposure gain
    std::vector<int> lights;  // light intensities
    int focus_pos;  // focus position

    void Print() const {
        printf("Exp time=%d, Exp gain=%d, Lights=(", exp_time, exp_gain);
        for (auto &light : lights) {
            printf("%d,", light);
        }
        printf("); Focus pos=%d\n", focus_pos);
    }

    std::string Str() const {
        std::stringstream ss;
        ss << exp_time << "-" << exp_gain << "-|";
        for (auto &light : lights) {
            ss << light << "|";
        }
        ss << "-" << focus_pos;
        return ss.str();
    }
};

/** @brief Phases of the AT.*/
enum ATPhase {
    AEQT,  // Quick Tuning Exposure params (with lights)
    AEST,  // Step Tuning Exposure params (with lights)
    AF,    // Auto Focus
    REFINE,  // AT refine，在定位到码区的情况下，先ae调整码区亮度，再跑af， 都是使用码区位置来做
    AR,    // Auto Recognition (barcode)
    END,
};

/** @brief Base class of AT.*/
class ATImplBase{
public:
    /* Order of the all phases. */
    std::vector<ATPhase> pipeline;
    std::vector<ATPhase>::iterator phase;

    /* Variable that stores the camera parameters. */
    CamParams next_params, best_params;

    /* Variable that stores the barcode parameters. */
    ARInfo ar_info;

public:
    ATImplBase() = default;

    ~ATImplBase() = default;

//    virtual void Init(CamConf &cam_conf, bool enable_al, bool enable_af, bool enable_ae) = 0;

    virtual void Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
              bool enable_al, bool enable_af, bool enable_ae, bool enable_ar) = 0;

    virtual void SequentialExec(const cv::Mat &image) = 0;

    virtual void UpdateNextParams() = 0;

    virtual void SetRoi(const cv::Rect &roi) = 0;
};

#endif  // AT_PARAMS_H
