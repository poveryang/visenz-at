#include "at_interface.h"
#include "at_vs_impl.h"
#include "version.h"

using namespace at;

ATInterface::ATInterface(bool en_hmap) {
    at_impl_ = std::make_shared<AT4VsImpl>(en_hmap);
}

void ATInterface::Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper, bool en_al, bool en_af, bool en_ae, bool en_ar) {
    cam_conf.min_eg = 16;  // TODO: fixed the min EG to 16 temporarily
    cam_conf.eg_quant_scale = 0.0625;  // TODO: fixed the EG scale to 0.0625 temporarily

    at_impl_->Init(cam_conf, barcode_wrapper, en_al, en_af, en_ae, en_ar);
}

bool ATInterface::Run(const cv::Mat &image) {
    /* Execute the different phases sequentially */
    at_impl_->SequentialExec(image);

    /* Return true if the pipeline is finished or update the next params otherwise */
    if (*at_impl_->phase == END) {
        return true;
    } else {
        at_impl_->UpdateNextParams();
        return false;
    }
}

CamParams ATInterface::GetNextParams() {
    return at_impl_->next_params;
}

CamParams ATInterface::GetBestParams() {
    printf("[AT4VS] Get best params\n");
    at_impl_->best_params.Print();
    return at_impl_->best_params;
}

ARParams ATInterface::GetARParams() {
    return at_impl_->ar_params;
}

std::string ATInterface::GetVersion() {
    std::string at_version =
            std::to_string(AT_VERSION_MAJOR) + "." +
            std::to_string(AT_VERSION_MINOR) + "." +
            std::to_string(AT_VERSION_PATCH);
    return at_version;
}

void ATInterface::Init(CamConf &cam_conf, bool en_al, bool en_af, bool en_ae) {

}
