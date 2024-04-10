#include <opencv2/opencv.hpp>

#include "at_interface.h"
#include "smore_barcode.h"
#include "device_info.h"

#ifdef USE_SMVC
#include "cap_smvc.h"
#elif USE_V4L2
#include "cap_v4l2.h"
#endif


// Device information for different devices
std::map<std::string, DeviceInfo> dev_infos = {
        {"vs2000p", vs2000p_info},
        {"vs1000p", vs1000p_info}
};

void TestATOnline(const std::string& dev_name,
                  bool enable_hmap = true,
                  bool enable_al = false,
                  bool enable_af = true,
                  bool enable_ae = true,
                  bool enable_ar = false) {
    // Prepare capture object
    DeviceInfo dev_info = dev_infos[dev_name];
    CamCapture cap(dev_info.sensor_name, dev_info.sensor_width, dev_info.sensor_height, dev_info.sensor_format);

    // Initialize AT algorithm
    at::ATInterface at_obj(enable_hmap);

    // Set AR parameters and Barcode wrapper
    ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // Set enable flags of the four algorithms
    at_obj.Init(dev_info.cam_conf, barcode_wrapper, enable_al, enable_af, enable_ae, enable_ar);

    // Execute AT algorithm
    std::string at_version = at::ATInterface::GetVersion();
    printf(">>>>>===== AT version: %s <<<<<=====\n", at_version.c_str());
    printf(">>>>>===== AT has been started <<<<<=====\n");

    int iter = 0;
    bool end_iter = false;
    CamParams cam_params = at_obj.GetNextParams();

    while (!end_iter) {
        cam_params = at_obj.GetNextParams();
        cv::Mat img = cap.CapImg(cam_params);
        end_iter = at_obj.Run(img);
        cv::imwrite("/tmp/at_res/" + std::to_string(iter) + ".png", img);
        iter += 1;
    }

    // Get final image and print best parameters
    cam_params = at_obj.GetBestParams();
    ar_params = at_obj.GetARParams();
    cv::Mat final_img = cap.CapImg(cam_params);
    cv::imwrite("/tmp/at_res/" + std::to_string(iter) + ".png", final_img);

    ar_params.print();
    printf("best et = %d\n", cam_params.exp_time);
    printf("best eg = %d\n", cam_params.exp_gain);
    printf("best pos = %d\n", cam_params.focus_pos);
    printf("best lights = %d, %d, %d, %d\n",
           cam_params.lights[0],
           cam_params.lights[1],
           cam_params.lights[2],
           cam_params.lights[3]);

    printf(">>>>>===== AT has been ended <<<<<=====\n\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <device_name> [enable_hmap] [enable_al] [enable_af] [enable_ae] [enable_ar]\n", argv[0]);
        return 0;
    } else if (argc > 7) {
        std::string dev_name = argv[1];
        bool enable_hmap = argv[2][0] == '1';
        bool enable_al = argv[3][0] == '1';
        bool enable_af = argv[4][0] == '1';
        bool enable_ae = argv[5][0] == '1';
        bool enable_ar = argv[6][0] == '1';
        TestATOnline(dev_name, enable_hmap, enable_al, enable_af, enable_ae, enable_ar);
    } else {
        std::string dev_name = argv[1];
        TestATOnline(dev_name);
    }

    return 0;
}
