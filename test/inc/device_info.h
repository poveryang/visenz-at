#ifndef VISENZ_AT_DEVICE_H
#define VISENZ_AT_DEVICE_H

#include <opencv2/opencv.hpp>
#include <map>
#include <string>
#include <utility>
#include "at_base.h"

/**
 * @brief Device information structure containing sensor and camera configuration
 */
class DeviceInfo {
public:
    DeviceInfo(std::string sensor_name, const int format, const int width, const int height, CamConf conf) 
        : sensor_name_(std::move(sensor_name)),
          sensor_format_(format),
          sensor_width_(width),
          sensor_height_(height),
          cam_conf_(std::move(conf)) {
    }

    // Getters
    [[nodiscard]] const std::string& GetSensorName() const { return sensor_name_; }
    [[nodiscard]] int GetSensorFormat() const { return sensor_format_; }
    [[nodiscard]] int GetSensorWidth() const { return sensor_width_; }
    [[nodiscard]] int GetSensorHeight() const { return sensor_height_; }
    [[nodiscard]] const CamConf& GetCamConf() const { return cam_conf_; }

private:
    std::string sensor_name_;
    int sensor_format_;
    int sensor_width_;
    int sensor_height_;
    CamConf cam_conf_;
};

// Device configurations
const DeviceInfo vs1000p("vs1000p", 8, 1280, 800, {
    0, 1, 1, 20, 39000, 16, 255, 1, 1000, 32,
    {1, 1, 1, 1}, 1, 410, 20, 30, 5
});

const DeviceInfo vs1000p_2mp("vs1000p_2mp", 8, 1920, 1200, {
    0, 1, 1, 20, 39000, 16, 255, 1, 1000, 32,
    {1, 1, 1, 1}, 1, 410, 20, 30, 5
});

const DeviceInfo vs2000p("ar0822", 2, 3840, 2160, {
    0, 1, 1, 20, 50000, 1, 255, 1, 1000, 32,
    {1, 1, 1, 1}, 1, 410, 20, 30, 5
});

const DeviceInfo vs2000_5mp("vs2000_5mp", 8, 2448, 2048, {
    0, 1, 1, 20, 39000, 16, 255, 1, 1000, 32,
    {1, 1, 1, 1}, 1, 410, 20, 30, 5
});

const DeviceInfo vs2000_20mp("vs2000_20mp", 8, 5120, 3840, {
    0, 1, 1, 20, 100000, 16, 255, 1, 1000, 32,
    {1, 1, 1, 1}, 1, 410, 20, 30, 5
});

    
const DeviceInfo vs800p("sc132gs", 0, 1088, 1288, {
    0, 1, 1, 20, 40000, 1, 255, 1, 1000, 32,
    {1}, 1, 1023, 500, 60, 10
});

// Device lookup table
inline std::map<std::string, DeviceInfo> DEVICE_MAP = {
    {"vs1000p", vs1000p},
    {"vs1000p_2mp", vs1000p_2mp},
    {"vs2000p", vs2000p},
    {"vs2000_5mp", vs2000_5mp},
    {"vs2000_20mp", vs2000_20mp},
    {"vs800p", vs800p}
};

#endif //VISENZ_AT_DEVICE_H
