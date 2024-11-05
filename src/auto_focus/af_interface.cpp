/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "af_interface.h"


using namespace af;

AFInterface::AFInterface() 
{
    this->end_iter = false;
    this->next_pos = -1;
    this->best_pos = -1;
    this->start_fit = false;
}

void AFInterface::Init(AFConf &af_conf) 
{
    af_impl_ = std::make_shared<AFImpl>(af_conf);
    this->next_pos = af_impl_->next_pos;

    this->end_iter = false;
    this->best_pos = -1;
    this->start_fit = false;
}

void AFInterface::Run(const cv::Mat &img, const std::vector<cv::Rect> &rois) 
{
    /* Run the AF algorithm and update variables */
    af_impl_->Run(img, rois);

    this->start_fit = this->af_impl_->start_fit;

    next_pos = af_impl_->next_pos;
    this->end_iter = af_impl_->end_iter;
    if (this->end_iter) 
    {
        this->best_pos = af_impl_->best_pos;
    }
}

void AFInterface::ResetSamples()
{
    #ifdef BUILD_WITH_LOG
        std::cout << "af obj ResetSamples()" << std::endl;
    #endif
    this->end_iter = false;
    this->af_impl_->end_iter = false;

    this->af_impl_->ResetSamples();
    this->af_impl_->GetNextSample();
    this->next_pos = this->af_impl_->next_pos;

    #ifdef BUILD_WITH_LOG
        std::cout << "af interface reset, next pose " << this->next_pos << std::endl;
    #endif
}
