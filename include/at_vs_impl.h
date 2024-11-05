/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AT_VS_IMPL_H
#define AT_VS_IMPL_H

#include "at_base.h"
#include "af_interface.h"
#include "ae_interface.h"
#include <chrono>

class AT4VsImpl : public ATImplBase 
{
public:
    /* Object and conf of AF */
    af::AFInterface af_obj;
    af::AFConf af_conf;

    /* Object and conf of AE */
    ae::AEInterface ae_obj;
    ae::AEConf ae_conf;

public:
    /**
     * @brief AT4VsImpl constructor
     * @param enable_hmap flag to indicate whether the heatmap-generator is used
     */
    explicit AT4VsImpl(bool enable_hmap);

    /**
    * @brief AT4VsImpl destructor
    */
    ~AT4VsImpl() = default;

    /**
     * @brief Init the ATImplBase
     * @param cam_conf the camera configuration
     * @param barcode_wrapper the barcode wrapper
     * @param en_al flag to indicate whether the AL is used
     * @param en_af flag to indicate whether the AF is used
     * @param en_ae flag to indicate whether the AE is used
     * @param en_ar flag to indicate whether the AR is used
     */
    void Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
              bool en_al, bool en_af, bool en_ae, bool en_ar) override;

    /**
     * @brief Execute the AT process sequentially
     * @param image the input image
     */
    void SequentialExec(const cv::Mat &image) override;

    /**
     * @brief Update the next camera parameters
     */
    void UpdateNextParams() override;

    /**
     * @brief Load camera configuration to submodules
     * @param cam_conf the camera configuration
     */
    void LoadCamConf(CamConf &cam_conf);

    void SetRoi(const cv::Rect &roi) override;

private:
    bool enable_hmap_;

    BarcodeWrapperBase *barcode_wrapper_{};
    cv::Rect at_roi;
    bool run_decode_finish = false;
    std::vector<cv::Rect> code_regions;
    bool enable_al = false;
    bool enable_ae = false;
    bool enable_af = false;
    cv::Mat cached_image;
    
    std::vector<int> ae_target_brt;     // aest时的目标亮度，此时这个值是对于全图的
    int ae_target_brt_idx;

    std::vector<int> refine_code_brt;   // refine时的目标亮度，此时这个值是对于码区的
    int refine_code_brt_idx;
    bool refine_ae_finish;

    bool af_take_onemore;
    bool af_take_onemore_finish;

    std::vector<int> base_lights;
    int base_exp_time;
    int base_exp_gain;
    int base_focus;
    bool set_base;
    bool using_base;

    int aest_statistics_num;        // aest时，每个亮度测试次数
    int aest_statistics_cur;        // aest时，当前计数
    int aest_detect_success_count;  // aest时，成功检测到码的次数，暂时没有用
    int aest_detect_thre;           // 用于提前退出aest的阈值
    cv::Mat aest_regions_canvas;    // 用于记录aest成功detect的位置
    std::map<int, CamParams> aest_params_map;  // 记录每次aest的参数

    int refine_statistics_num;    // refine 时，每个亮度测试次数
    float refine_max_decode_rate; // refine 时，轮询出来的最大解码成功次数
    float refine_statistics_cur;  // refine 时，当前测试次数
    float refine_decode_success_count; // refine 时，统计的解码成功次数
};

#endif //AT_VS_IMPL_H
