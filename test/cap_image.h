#ifndef CAP_IMAGE_H
#define CAP_IMAGE_H
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "v4l2Capture.h"
#include "at_params.h"

void InitCap(at::CamParams& cam_params, int raw_bit=8);
cv::Mat ALCapImg(int light, int raw_bit=8);
cv::Mat AECapImg(int cur_exp, int cur_gain, int raw_bit=8);
cv::Mat AFCapImg(int cur_focus, int raw_bit=8);
cv::Mat ATCapImg(at::CamParams& cam_params, int raw_bit=8);

#endif  // CAP_IMAGE_H
