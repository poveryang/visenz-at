/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-05-06 16:47:49
 * @Copyright: Copyright (c) 2022
 */
#include <opencv2/opencv.hpp>

#include "at_interface.h"
#include "device_info.h"
#include <string>

#ifdef USE_SMVC
#include "cap_smvc.h"
#elif USE_V4L2
#include "cap_v4l2.h"
#elif USE_NOVAIC
#include "cap_aarch32.h"
#endif
#include <chrono>

std::map<std::string, DeviceInfo> dev_infos = {
        {"vs2000p", vs2000p_info},
        {"vs1000p", vs1000p_info},
        {"vs1000p200w", vs1000p_200w_info},
        {"vs2000500w", vs2000_500w_info},
        {"vs20002000w", vs2000_2000w_info},
        {"vs800p", vs800p_info}
};

int main()
{
    DeviceInfo dev_info = dev_infos["vs1000p"];
#ifdef USE_V4L2
    CamCapture cap(dev_info.sensor_name, dev_info.sensor_width, dev_info.sensor_height, dev_info.sensor_format);
#elif USE_NOVAIC
    CamCapture cap(std::string("vs800p"));
    cap.SetLightState(1);
#endif

    CamParams cam_params;
    // cam_params.exp_time = 500;
    cam_params.exp_gain = 32;
    cam_params.focus_pos = 500;
    cam_params.lights = {1};

    for (int i = 1; i < 39000; i+=1000)
    {
        cam_params.exp_time = i;
        cv::Mat img = cap.CapImg(cam_params);
        std::string img_name = "./exp_test/" + std::to_string(i) + ".png";
        cv::imwrite(img_name, img);
    }   
}