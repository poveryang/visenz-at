#include "cap_v4l2.h"


CamCapture::CamCapture(const std::string &sensor_name,
                       int sensor_width,
                       int sensor_height,
                       int sensor_format,
                       int sensor_fps) {
    // Initialize camera
    vcap.openDevice();
    vcap.initDevice(sensor_fps, sensor_width, sensor_height, sensor_format);
    vcap.startCapture();
    vcap.setLightAim(0);
    vcap.setStrobeEnable(0);
}

CamCapture::~CamCapture() {
    vcap.stopCapture();
    vcap.closeDevice();
}

cv::Mat CamCapture::CapImg(CamParams &cam_params) const {
    // Set camera parameters
    for (int i = 0; i < cam_params.lights.size(); i++) {
        vcap.setLightBright(i, cam_params.lights[i]);
    }
    vcap.setExposure(cam_params.exp_time);
    vcap.setGain(cam_params.exp_gain);
    vcap.setLensFocus(cam_params.focus_pos);

    // Get frame buffer
    vcap.setStrobeEnable(1);
    char *frame_buf = nullptr;
    unsigned long frame_size = 0;
    sensorParam sensor_params{};
    while (true) {
        int ret = vcap.getNewestFrame(reinterpret_cast<void **>(&frame_buf),
                                      static_cast<size_t *>(&frame_size), 2000000);
        sensor_params = vcap.getCurrentFrameSensorParam();
        if (ret < 0) {
            printf("Get Newest Frame Error!");
            exit(1);
        }
        if (sensor_params.active == 1 &&
            sensor_params.lightBright[0] == cam_params.lights[0] &&
            sensor_params.lightBright[1] == cam_params.lights[1] &&
            sensor_params.lightBright[2] == cam_params.lights[2] &&
            sensor_params.lightBright[3] == cam_params.lights[3] &&
            sensor_params.exposure == cam_params.exp_time &&
            sensor_params.gain == cam_params.exp_gain &&
            sensor_params.focus == cam_params.focus_pos) {
            vcap.setStrobeEnable(0);  // Disable strobe when capture finished
            break;
        } else {
            vcap.backFrame();
        }
    }

    // Convert frame to cv::Mat
    int img_width = vcap.getWidth();
    int img_height = vcap.getHeight();
    cv::Mat image = cv::Mat(cv::Size(img_width, img_height), CV_8UC1, (void *) frame_buf);
    vcap.backFrame();

    return image;
}
