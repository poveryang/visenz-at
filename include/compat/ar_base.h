#pragma once

#include <opencv2/core.hpp>

#include <array>
#include <iostream>
#include <vector>

// Optional product barcode SDK. Define AT_COMPAT_USE_BARCODE_SDK when available.
#if defined(AT_COMPAT_USE_BARCODE_SDK)
#if defined(__has_include)
#if __has_include("barcode.h")
#include "barcode.h"
#define AT_COMPAT_HAS_BARCODE_SDK 1
#endif
#endif
#endif

#if defined(AT_COMPAT_HAS_BARCODE_SDK)
using AtCompatBarcodeType = smartmore::barcode::BarcodeType;
#else
using AtCompatBarcodeType = int;
#endif

/** AR / decode result summary (legacy product contract). */
struct ARInfo {
    std::vector<AtCompatBarcodeType> successful_code_type;
    std::array<int, 2> qr_version{{-1, -1}};
    std::array<int, 2> dm_version{{-1, -1}};

    void reset()
    {
        successful_code_type.clear();
        qr_version = {-1, -1};
        dm_version = {-1, -1};
    }

    void print() const
    {
        std::cout << "qr version: " << qr_version[0] << std::endl;
        std::cout << "dm version: [" << dm_version[0] << ", " << dm_version[1] << "]" << std::endl;
    }
};

/** Virtual barcode wrapper used by legacy product software. */
class BarcodeWrapperBase {
public:
    virtual ~BarcodeWrapperBase() = default;
    virtual void Reset() = 0;
    virtual std::vector<cv::Rect> Decode(const cv::Mat &image, ARInfo &out_arinfo, const cv::Rect &roi) = 0;
    virtual void SetOriginParams() = 0;
};
