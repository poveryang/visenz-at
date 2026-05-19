#include "cap_smvc.h"

CamCapture::CamCapture(const std::string &sensor_name,
                       int sensor_width,
                       int sensor_height,
                       int sensor_format,
                       int sensor_fps) {

    // Initialize camera
    camera_number = sm_camera_init(sensor_name.c_str(),
                                   sensor_width,
                                   sensor_height,
                                   sensor_format,
                                   sensor_fps);

    this->sensor_width = sensor_width;
    this->sensor_height = sensor_height;
}

CamCapture::~CamCapture() {
    // Un-initialize camera
    sm_camera_uninit(camera_number);
}

cv::Mat CamCapture::CapImg(CamParams& cam_params) const {
    // Set camera parameters
    sm_set_isp_ae_manual(cam_params.exp_time, cam_params.exp_gain);
    sm_set_focus_value(camera_number, cam_params.focus_pos);
    int light_num = sm_get_light_num_rk3588();
    for (int i = 0; i < light_num; i++) {
        sm_prepare_light_state_rk3588(i, cam_params.lights[i]);
    }


    // Get frame buffer
    sm_video_frame sm_frame = {0};
    int size;
    for (int i=0; i<2; i++) {
        size = get_frame(camera_number, &sm_frame);
        printf("Get frame buffer successfully, frame size: %d\n", size);
        free_frame(camera_number, &sm_frame);
    }
    size = get_frame(camera_number, &sm_frame);

    // Convert frame to cv::Mat
    void *frame_buf = malloc(size * 2/3);
    memcpy(frame_buf, sm_frame.out_buf[0], size * 2/3);

    cv::Mat image;
    image = cv::Mat(sensor_height, sensor_width, CV_8UC1, frame_buf);

    // Free frame buffer
    free_frame(camera_number, &sm_frame);

    return image;
}

//int main(int argc, char **argv) {
//    // Parse arguments
//    if (argc < 5) {
//        printf("Usage: %s exp_time exp_gain focus_pos light_state\n", argv[0]);
//    }
//
//    int exp_time = std::stoi(argv[1]);
//    int exp_gain = std::stoi(argv[2]);
//    int focus_pos = std::stoi(argv[3]);
//    int light_state = std::stoi(argv[4]);
//
//    // Set camera parameters
//    CamParams cam_params;
//    cam_params.exp_time = exp_time;
//    cam_params.exp_gain = exp_gain;
//    cam_params.focus_pos = focus_pos;
//    cam_params.light_state = light_state;
//
//    // Initialize camera
//    std::string sensor_name = "ar0822";
//    int sensor_width = 3840;
//    int sensor_height = 2160;
//    SM_FMT_TYPE sensor_format = SM_FMT_RAW12;
//
//    // Capture image
//    CamCapture cap = CamCapture(sensor_name, sensor_width, sensor_height, 2, 0);
//    // TODO: set light state outside of the class temporarily
//    int light_num = sm_get_light_num_rk3588();
//    for (int i = 0; i < light_num; i++) {
//        sm_prepare_light_state_rk3588(i, cam_params.light_state);
//    }
//
//    cv::Mat img = cap.CapImg(cam_params);
//    cv::imwrite("./image.png", img);
//}
