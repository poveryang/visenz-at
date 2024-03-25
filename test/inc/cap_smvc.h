#ifndef VISENZ_AT_CAP_SMVC_H
#define VISENZ_AT_CAP_SMVC_H

#include <opencv2/opencv.hpp>
#include "at_base.h"

extern "C" {
#include "sm_video_capture.h"
}

class CamCapture {
public:
    explicit CamCapture(const std::string &sensor_name,
                        int sensor_width,
                        int sensor_height,
                        int sensor_format,
                        int sensor_fps=0);

    ~CamCapture();

    cv::Mat CapImg(CamParams& cam_params) const;

    int camera_number;
    int sensor_width;
    int sensor_height;
};

#endif //VISENZ_AT_CAP_SMVC_H
