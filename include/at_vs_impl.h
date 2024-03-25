#ifndef AT_VS_IMPL_H
#define AT_VS_IMPL_H

#include "at_base.h"
#include "af_interface.h"
#include "ae_interface.h"
#include "hmap_infer_base.h"

class AT4VsImpl : public ATImplBase {
public:
    /* Object and conf of AF */
    af::AFInterface af_obj;
    af::AFConf af_conf;

    /* Object and conf of AE */
    ae::AEInterface ae_obj;
    ae::AEConf ae_conf;

    /* Object and params of AR */
    ARInterface ar_obj;

    /* Object of Hmap */
    std::shared_ptr<HMapInferBase> hmap_obj;

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

private:
    bool enable_hmap_;
};

#endif //AT_VS_IMPL_H
