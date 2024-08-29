/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#include "at_interface.h"
#include "at_vs_impl.h"
#include "version.h"

using namespace at;

ATInterface::ATInterface(bool en_hmap) 
{
    at_impl_ = std::make_shared<AT4VsImpl>(en_hmap);
}

void ATInterface::Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
                       bool en_al, bool en_af, bool en_ae, bool en_ar) 
{
    std::cout << " ====== AT Version: " << this->GetVersion() << " ======" << std::endl;
    at_impl_->Init(cam_conf, barcode_wrapper, en_al, en_af, en_ae, en_ar);
}

bool ATInterface::Run(const cv::Mat &image) 
{
    /* Execute the different phases sequentially */
    at_impl_->SequentialExec(image);

    /* Return true if the pipeline is finished or update the next params otherwise */
    if (*at_impl_->phase == END) 
    {
        return true;
    } 
    else 
    {
        at_impl_->UpdateNextParams();
        // if(*at_impl_->phase == END)
        // {
        //     return true;
        // }
        return false;
    }
}

CamParams ATInterface::GetNextParams() 
{
    return at_impl_->next_params;
}

CamParams ATInterface::GetBestParams() 
{
    return at_impl_->best_params;
}

void ATInterface::SetRoi(const cv::Rect &roi)
{
    this->at_impl_->SetRoi(roi);
}

ARInfo ATInterface::GetARInfo() 
{
    return at_impl_->ar_info;
}

std::string ATInterface::GetVersion() 
{
    std::string at_version = 
            std::to_string(AT_VERSION_MAJOR) + "." +
            std::to_string(AT_VERSION_MINOR) + "." +
            std::to_string(AT_VERSION_PATCH);
    return at_version;
}

void ATInterface::Init(CamConf &cam_conf, bool en_al, bool en_af, bool en_ae) 
{
}
