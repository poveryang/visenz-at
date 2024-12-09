/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AT_PARAMS_H
#define AT_PARAMS_H

#include "ar_base.h"
#include <opencv2/opencv.hpp>

/** @brief Camera configuration.*/
struct CamConf {
    /** Fill Light control*/
    int min_intensity;                  // 关灯 0
    int max_intensity;                  // 开灯 1

    /** Exposure control*/
    int min_et_step;                    // 曝光时间的最小步长
    int min_et;                         // 从相机获得的最小曝光时间
    int max_et;                         // 从相机获得的最大曝光时间
    int min_eg;                         // 从相机获得的最小增益
    int max_eg;                         // 从相机获得的最大增益
    int ae_mode;                        // ae的模式，默认 1
    int init_et;                        // AT 初始化的曝光时间，不同相机不同，可设置
    int init_eg;                        // AT 初始化的增益，不同相机不同，可设置
    std::vector<int> init_intensities;  // AT 初始化灯光

    /** Focus control*/
    int start_pos;                      // 从相机获得的最小焦距
    int end_pos;                        // 从相机获得的最大焦距    
    int init_pos;                       // AT 初始化的焦距，不同相机不同，可设置
    int coarse_af_step;                 // 粗对焦时的焦距步长，不同相机不同，可设置 
    int refine_af_step;                 // 精对焦是的焦距步长，不同相机不同，可设置
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
