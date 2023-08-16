#ifndef DRIVER_CAP_H
#define DRIVER_CAP_H

#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>

#include "v4l2Capture.h"
#include "at_params.h"


void InitCap(CamConf &cam_conf, CamParams &cam_params, int raw_bit = 8);

cv::Mat CapImg(CamParams &cam_params, int raw_bit = 8);

void CloseCap();

cv::Mat AFCapImg(int focus_pos, int raw_bit = 8);

cv::Mat AECapImg(int exp_time, int exp_gain, int raw_bit = 8);

cv::Mat ALCapImg(std::vector<int> lights, int raw_bit = 8);


#endif  // DRIVER_CAP_H
