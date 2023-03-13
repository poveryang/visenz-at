#ifndef DRIVER_CAP_H
#define DRIVER_CAP_H

#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "v4l2Capture.h"


struct DCapParams {
    /** Light intensities*/
    std::vector<int> lights;

    /** Focus position*/
    int focus_pos;

    /** Exposure time*/
    int exp_time;

    /** Exposure gain*/
    int exp_gain;

    /** ROI of the image*/
    cv::Rect2i roi;
};


void InitCap(DCapParams &cam_params, int raw_bit = 8);

cv::Mat CapImg(DCapParams &cam_params, int raw_bit = 8);

#endif  // DRIVER_CAP_H
