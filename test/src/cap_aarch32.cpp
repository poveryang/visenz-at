#include "cap_aarch32.h"


CamCapture::CamCapture(const std::string& dev_name) {
    // Get sensor parameters
    sensor_t sensor_params = sensor_map[dev_name];
    std::string sensor_name = sensor_params.name;
    sensor_width = sensor_params.width;
    sensor_height = sensor_params.height;
    int sensor_format = sensor_params.format;
    int sensor_fps = sensor_params.fps;

    // Initialize camera
    camera_number = sm_camera_init(sensor_name.c_str(),
                                   sensor_width,
                                   sensor_height,
                                   sensor_format,
                                   sensor_fps);
}

CamCapture::~CamCapture() {
    // Un-initialize camera
    sm_camera_uninit(camera_number);

}

cv::Mat CamCapture::CapImg(CamParams &cam_params) const {
    // Set camera parameters
    sm_set_isp_ae_manual(cam_params.exp_time, cam_params.exp_gain);
    sm_set_focus_value(camera_number, cam_params.focus_pos);

    // Get frame buffer
    SM_VIDEO_FRAME buf = {0};
    buf.total_size = sizeof(SM_VIDEO_FRAME);
    get_frame(camera_number, &buf);

    // Convert frame to cv::Mat
    void *frame_buf = malloc(buf.buf_size[0]);
    memcpy(frame_buf, buf.out_buf[0], buf.buf_size[0]);

    cv::Mat image = cv::Mat(cv::Size(sensor_width, sensor_height), CV_8UC1, frame_buf);

    // Free frame buffer
    free_frame(camera_number, &buf);

    return image;
}
