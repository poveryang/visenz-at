#ifndef VISENZ_AT_DEVICE_INFO_H
#define VISENZ_AT_DEVICE_INFO_H

#include <opencv2/opencv.hpp>

#include "at_base.h"

/** @brief Camera configuration.*/
struct DeviceInfo {
    /** Sensor info*/
    std::string sensor_name;            // sensor name
    int sensor_format;                  // sensor format
    int sensor_width;                   // sensor width
    int sensor_height;                  // sensor height

    /** Camera configuration*/
    CamConf cam_conf;

};

DeviceInfo vs2000p_info = {
        .sensor_name = "ar0822",
        .sensor_format = 2,  // SM_FMT_RAW12
        .sensor_width = 3840,
        .sensor_height = 2160,
        .cam_conf = {
                .img_width = 3840,
                .img_height = 2160,
                .roi = cv::Rect2d(0, 0, 0, 0),
                .max_lt = 2000,
                .min_intensity = 0,
                .max_intensity = 1,
                .min_et = 20,
                .max_et = 50000,
                .min_eg = 1,
                .max_eg = 255,
                .eg_quant_scale = 0.0625,
                .lens_type = 0,
                .start_pos = 0,
                .end_pos = 410,
                .ae_mode = 0,
                .init_et = 10000,
                .init_eg = 1,
                .init_intensities = {1, 1, 1, 1},
                .init_pos = 1,
        }

};


DeviceInfo vs1000p_info = {
        .sensor_name = "vs1000p",
        .sensor_format = 8,  // SM_FMT_RAW12
        .sensor_width = 1280,
        .sensor_height = 800,
        .cam_conf = {
                .img_width = 1280,
                .img_height = 800,
                .roi = cv::Rect2d(0, 0, 0, 0),
                .max_lt = 2000,
                .min_intensity = 1,
                .max_intensity = 24,
                .min_et = 20,
                .max_et = 39000,
                .min_eg = 16,
                .max_eg = 255,
                .eg_quant_scale = 0.0625,
                .lens_type = 0,
                .start_pos = 0,
                .end_pos = 410,
                .ae_mode = 0,
                .init_et = 1000,
                .init_eg = 16,
                .init_intensities = {24, 24, 24, 24},
                .init_pos = 1,
        }

};

#endif //VISENZ_AT_DEVICE_INFO_H
