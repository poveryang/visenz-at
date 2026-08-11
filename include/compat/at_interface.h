#pragma once

#include "ar_base.h"
#include "at_base.h"

#include <memory>
#include <string>

namespace at {

/**
 * Legacy product-facing AT API.
 * Internally bridges to AtOrchestrator / AtSession (new algorithm core).
 *
 * Expected call sequence:
 *   Init(...)
 *   loop:
 *     GetNextParams() -> set camera -> capture
 *     finished = Run(image)
 *   GetBestParams() / GetARInfo()
 */
class ATInterface {
public:
    ATInterface();
    explicit ATInterface(bool en_hmap);
    ~ATInterface();

    ATInterface(const ATInterface &) = delete;
    ATInterface &operator=(const ATInterface &) = delete;
    ATInterface(ATInterface &&) noexcept;
    ATInterface &operator=(ATInterface &&) noexcept;

    /** VN/VA style init without barcode wrapper. */
    void Init(CamConf &cam_conf, bool en_al, bool en_af, bool en_ae);

    /** VS style init with barcode wrapper. */
    void Init(CamConf &cam_conf,
              BarcodeWrapperBase &barcode_wrapper,
              bool en_al,
              bool en_af,
              bool en_ae,
              bool en_ar);

    /** Feed one captured frame; returns true when AT finished. */
    bool Run(const cv::Mat &image);

    void SetRoi(const cv::Rect &roi);

    CamParams GetNextParams();
    CamParams GetBestParams();
    ARInfo GetARInfo();

    static std::string GetVersion();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace at
