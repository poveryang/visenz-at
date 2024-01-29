#ifndef CAP_IMAGE_H
#define CAP_IMAGE_H
#include <opencv2/opencv.hpp>
#include "at_params.h"

#include "v4l2Capture.h"

// Sensor definition
struct sensor_t {
    int fps;
    int width;
    int height;
    int raw_bit;
};


class CamCapture {
public:
    explicit CamCapture(const std::string& dev_name);

    ~CamCapture();

    cv::Mat CapImg(at::CamParams& cam_params) const;

public:
    V4L2Capture& vcap = V4L2Capture::getInstance();

    // Sensor map
    std::map<std::string, sensor_t> sensor_map =
            {
                    {"vs800", {30, 1280, 800, 8}},
                    {"vs1000p", {30, 1280, 800, 8}}
            };

};


void InitCap(at::CamParams& cam_params, int raw_bit=8);
cv::Mat ALCapImg(int light, int raw_bit=8);
cv::Mat AECapImg(int cur_exp, int cur_gain, int raw_bit=8);
cv::Mat AFCapImg(int cur_focus, int raw_bit=8);
cv::Mat ATCapImg(at::CamParams& cam_params, int raw_bit=8);

#endif  // CAP_IMAGE_H
