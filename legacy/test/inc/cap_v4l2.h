#ifndef VISENZ_AT_CAP_V4L2_H
#define VISENZ_AT_CAP_V4L2_H

#include <opencv2/opencv.hpp>
#include "at_base.h"

#include "v4l2Capture.h"

class CamCapture {
public:
    explicit CamCapture(const std::string &sensor_name,
                        int sensor_width,
                        int sensor_height,
                        int sensor_format,
                        int sensor_fps=0);

    ~CamCapture();

    cv::Mat CapImg(CamParams& cam_params) const;

public:
    V4L2Capture& vcap = V4L2Capture::getInstance();
};

#endif  // VISENZ_AT_CAP_V4L2_H
