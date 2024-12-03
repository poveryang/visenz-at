/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "ae_interface.h"

using namespace ae;

AEInterface::AEInterface() 
{
    this->end_qt = false;
    this->ae_fail = false;
    this->exceed_tunning_count = false;
}

void AEInterface::Init(AEConf &ae_cam_conf, bool en_al) 
{
    this->end_qt = false;
    this->ae_fail = false;
    ae_impl_ = std::make_shared<AEImpl>(ae_cam_conf, en_al);
    params_next = ae_impl_->params_next;
}

void AEInterface::QuickTune(const cv::Mat &image, int brt_target, const std::vector<cv::Rect> &rois, int brt_diff_thre, bool enable_switch, float fraction) 
{
    /* Quick tuning */
    this->end_qt = ae_impl_->QuickTune(image, brt_target, rois, brt_diff_thre, enable_switch, fraction);
    params_next = ae_impl_->params_next;
    this->ae_fail = ae_impl_->ae_fail;
    this->exceed_tunning_count = ae_impl_->exceed_tunning_count;
    if (this->end_qt) 
    {
        params_best = ae_impl_->params_best;
    }
}

bool AEInterface::UpdateLights()
{
    #ifdef BUILD_WITH_LOG
        std::cout << "AEInterface::UpdateLights()" << std::endl;
    #endif
    // lights 更新失败，说明已经没有灯光组合
    if (!this->ae_impl_->UpdateLights())
    {
        this->ae_fail = true;
        params_next = ae_impl_->params_next;
        return false;
    }
    params_next = ae_impl_->params_next;

    return true;
}

void AEInterface::ClearState()
{
    this->ae_impl_->ae_fail = false;
    this->ae_fail = false;
}

void AEInterface::ResetTunningCount()
{
    this->exceed_tunning_count = false;
    this->ae_impl_->exceed_tunning_count = false;
}

void AEInterface::SetParam(int exp_time, int exp_gain, std::vector<int> lights)
{
    this->params_best.exp_time = exp_time;
    this->params_best.exp_gain = exp_gain;
    this->params_best.lights = lights;

    this->params_next.exp_time = exp_time;
    this->params_next.exp_gain = exp_gain;
    this->params_next.lights = lights;

    this->ae_impl_->params_best.exp_time = exp_time;
    this->ae_impl_->params_best.exp_gain = exp_gain;
    this->ae_impl_->params_best.lights = lights;

    this->ae_impl_->params_next.exp_time = exp_time;
    this->ae_impl_->params_next.exp_gain = exp_gain;
    this->ae_impl_->params_next.lights = lights;
}