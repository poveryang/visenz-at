#include "cap_image.h"

#define RESOLUTION_WIDTH 1280
#define RESOLUTION_HEIGHT 800

#define FRAME_RATE 30
unsigned long frame_size ;

void InitCap(at::ATConfig::HDParams& cam_params, int raw_bit){

    V4L2Capture& vcap = V4L2Capture::getInstance();

    vcap.openDevice();
    vcap.initDevice(FRAME_RATE, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, raw_bit);
    vcap.startCapture();
    vcap.setLightAim(0);

    vcap.setStrobeEnable(1);
    vcap.setLightBright(0, cam_params.lights[0]);
    vcap.setLightBright(1, cam_params.lights[1]);
    vcap.setLightBright(2, cam_params.lights[2]);
    vcap.setLightBright(3, cam_params.lights[3]);
    vcap.setExposure(cam_params.exposure);
    vcap.setGain(cam_params.gain);
    vcap.setLensFocus(cam_params.motor);
}

cv::Mat ATCapImg(at::ATConfig::HDParams& cam_params, int raw_bit)
{
    V4L2Capture& vcap = V4L2Capture::getInstance();

    char *frame_buf = static_cast<char *>(malloc(RESOLUTION_WIDTH * RESOLUTION_HEIGHT));
    sensorParam sensor_params{};
    vcap.setLightBright(0, cam_params.lights[0]);
    vcap.setLightBright(1, cam_params.lights[1]);
    vcap.setLightBright(2, cam_params.lights[2]);
    vcap.setLightBright(3, cam_params.lights[3]);

    vcap.setLensFocus(cam_params.motor);
    vcap.setExposure(cam_params.exposure);
    vcap.setGain(cam_params.gain);

    while (true){
        int ret = vcap.getNewestFrame(reinterpret_cast<void **>(&frame_buf),
                                      static_cast<size_t *>(&frame_size), 2000000);
        sensor_params = vcap.getCurrentFrameSensorParam();
        if (ret < 0){
            printf("Get Newest Frame Error!");
            exit(1);
        }
        if (sensor_params.active == 1 &&
            sensor_params.lightBright[0] == cam_params.lights[0] &&
            sensor_params.exposure == cam_params.exposure &&
            sensor_params.gain == cam_params.gain &&
            sensor_params.focus == cam_params.motor){
            break;
        } else{
            vcap.backFrame();
        }
    }

    cv::Mat frame = cv::Mat(cv::Size(RESOLUTION_WIDTH, RESOLUTION_HEIGHT),
                            raw_bit == 8 ? CV_8UC1 : CV_16UC1, (void*)frame_buf);
    vcap.backFrame();
    return frame;
}