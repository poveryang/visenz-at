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
                .min_intensity = 0,                 
                .max_intensity = 1,                    
                .min_et = 20,                         
                .max_et = 50000,
                .min_eg = 1,
                .max_eg = 255,
                .ae_mode = 1,
                .init_et = 10000,
                .init_eg = 1,
                .init_intensities = {1, 1, 1, 1},
                .start_pos = 1,
                .end_pos = 410,
                .init_pos = 20,
                .coarse_af_step = 30,
                .refine_af_step = 5,
        }

};


DeviceInfo vs1000p_info = {
        .sensor_name = "vs1000p",
        .sensor_format = 8,  // SM_FMT_RAW12
        .sensor_width = 1280,
        .sensor_height = 800,
        .cam_conf = {
                .min_intensity = 0,                     // 关灯 0
                .max_intensity = 1,                     // 开灯 1
                .min_et = 20,                           // 从相机获得的最小曝光时间
                .max_et = 39000,                        // 从相机获得的最大曝光时间
                .min_eg = 16,                           // 从相机获得的最小增益
                .max_eg = 255,                          // 从相机获得的最大增益
                .ae_mode = 1,                           // ae的模式，默认 1
                .init_et = 1000,                        // AT 初始化的曝光时间，不同相机不同，可设置
                .init_eg = 32,                          // AT 初始化的增益，不同相机不同，可设置
                .init_intensities = {1, 1, 1, 1},       // AT 初始化灯光
                .start_pos = 1,                         // 从相机获得的最小焦距
                .end_pos = 410,                         // 从相机获得的最大焦距    
                .init_pos = 20,                         // AT 初始化的焦距，不同相机不同，可设置
                .coarse_af_step = 30,                   // 粗对焦时的焦距步长，不同相机不同，可设置 
                .refine_af_step = 5,                    // 精对焦是的焦距步长，不同相机不同，可设置
        }

};

DeviceInfo vs1000p_200w_info = {
        .sensor_name = "vs1000p",
        .sensor_format = 8,  // SM_FMT_RAW12
        .sensor_width = 1920,
        .sensor_height = 1200,
        .cam_conf = {
                .min_intensity = 0,
                .max_intensity = 1,
                .min_et = 20,
                .max_et = 39000,
                .min_eg = 16,
                .max_eg = 255,
                .ae_mode = 1,
                .init_et = 1000,
                .init_eg = 32,
                .init_intensities = {1, 1, 1, 1},
                .start_pos = 1,
                .end_pos = 410,
                .init_pos = 20,
                .coarse_af_step = 30,
                .refine_af_step = 5,
        }

};

DeviceInfo vs2000_500w_info = {
        .sensor_name = "vs1000p",
        .sensor_format = 8,  // SM_FMT_RAW12
        .sensor_width = 2448,
        .sensor_height = 2048,
        .cam_conf = {
                .min_intensity = 0,
                .max_intensity = 1,
                .min_et = 20,
                .max_et = 39000,
                .min_eg = 16,
                .max_eg = 255,
                .ae_mode = 1,
                .init_et = 1000,
                .init_eg = 32,
                .init_intensities = {1, 1, 1, 1},
                .start_pos = 1,
                .end_pos = 410,
                .init_pos = 20,
                .coarse_af_step = 30,
                .refine_af_step = 5,
        }

};

DeviceInfo vs2000_2000w_info = {
        .sensor_name = "vs1000p",
        .sensor_format = 8,  // SM_FMT_RAW12
        .sensor_width = 5120,
        .sensor_height = 3840,
        .cam_conf = {
                .min_intensity = 0,
                .max_intensity = 1,
                .min_et = 20,
                .max_et = 100000,
                .min_eg = 16,
                .max_eg = 255,
                .ae_mode = 1,
                .init_et = 1000,
                .init_eg = 32,
                .init_intensities = {1, 1, 1, 1},
                .start_pos = 1,
                .end_pos = 410,
                .init_pos = 20,
                .coarse_af_step = 30,
                .refine_af_step = 5,
        }

};

DeviceInfo vs800p_info = {
        .sensor_name = "sc132gs",
        .sensor_format = 0,  // SM_FMT_RAW12
        .sensor_width = 1088,
        .sensor_height = 1288,
        .cam_conf = {
                .min_intensity = 0,
                .max_intensity = 1,
                .min_et = 20,
                .max_et = 40000,
                .min_eg = 1,
                .max_eg = 255,
                .ae_mode = 1,
                .init_et = 1000,
                .init_eg = 32,
                .init_intensities = {1},
                .start_pos = 1,
                .end_pos = 1023,
                .init_pos = 500,
                .coarse_af_step = 60,
                .refine_af_step = 10,
        }

};

#endif //VISENZ_AT_DEVICE_INFO_H
