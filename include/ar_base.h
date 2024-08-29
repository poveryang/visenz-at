/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AT_BARCODE_BASE_H
#define AT_BARCODE_BASE_H

#include "barcode.h"
#include <opencv2/opencv.hpp>


/** A struct that is used to store the AR parameters.*/
struct ARInfo {
    std::vector<smartmore::barcode::BarcodeType> successful_code_type;
    std::array<int, 2> qr_version = {-1, -1};
    std::array<int, 2> dm_version = {-1, -1};

    void reset() 
    {
        this->successful_code_type.clear();
        this->qr_version = {-1, -1};
        this->dm_version = {-1, -1};
    }

    void print()
    {
        std::cout << "qr version: " << this->qr_version[0] << std::endl;
        std::cout << "dm version: [" << this->dm_version[0] << ", " << this->dm_version[1] << "]" << std::endl;
    }
};

/** @brief This is a virtual base class of Barcode Wrapper.*/
class BarcodeWrapperBase {
public:
    virtual void Reset() = 0;

    virtual std::vector<cv::Rect> Decode(const cv::Mat &image, ARInfo &out_arinfo, const cv::Rect &roi) = 0;

    virtual void SetOriginParams() = 0;
};

#endif //AT_BARCODE_BASE_H
