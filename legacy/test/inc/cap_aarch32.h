#ifndef CAP_AARCH32_H
#define CAP_AARCH32_H

#include <opencv2/opencv.hpp>
#include "at_base.h"

extern "C" {
#include "sm_video_capture.h"
}

// Sensor definition
struct sensor_t {
    std::string name;
    int width;
    int height;
    int format;
    int fps;
};

class CamCapture {
public:
    explicit CamCapture(const std::string& dev_name);

    ~CamCapture();

    cv::Mat CapImg(CamParams& cam_params) const;

    void SetLightState(int enable) const;

public:
    int camera_number;
    int sensor_width;
    int sensor_height;
    // Sensor map
    std::map<std::string, sensor_t> sensor_map =
            {
                    {"vs600", {"ov9281", 1280, 800, HD_VIDEO_PXLFMT_RAW8, 60}},
                    {"vs800p", {"sc132gs", 1088, 1288, HD_VIDEO_PXLFMT_RAW8, 60}}
            };
};

#endif //CAP_AARCH32_H
