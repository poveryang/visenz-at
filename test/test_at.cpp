#include <opencv2/core.hpp>
#include "at_interface.h"
#include "smore_barcode.h"
#include "cam_config.h"

#ifdef AARCH64
#include "cap_aarch64.h"
#endif
#ifdef AARCH32
#include "cap_aarch32.h"
#endif


// Map of camera parameters
std::map<std::string, CamConf> cam_conf_map = {
        {"vs1000p", at::vs1000p_conf},
//        {"vs800", at::vs800_conf},
//        {"vs600", at::vs600_conf},
};

void TestATOnline(const std::string& dev_name) {
    // 1. Prepare camera parameters
    CamConf cam_conf = cam_conf_map[dev_name];

    // 2. Set AR parameters and Barcode wrapper
    ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // 3. Initialize AT algorithm
    bool enable_hmap = true;
    at::ATInterface at_obj(enable_hmap);

    // 4. Set enable flags of the four algorithms
    bool enable_al, enable_af, enable_ae, enable_ar;
    enable_al = false;
    enable_af = false;
    enable_ae = true;
    enable_ar = false;
    at_obj.Init(cam_conf, barcode_wrapper, enable_al, enable_af, enable_ae, enable_ar);

    // 5. Execute AT algorithm
    std::string at_version = at_obj.GetVersion();
    printf(">>>>>===== AT version: %s <<<<<=====\n", at_version.c_str());
    printf(">>>>>===== AT has been started <<<<<=====\n");

    int iter = 0;
    bool end_iter = false;
    CamParams cam_params = at_obj.GetNextParams();
    CamCapture cap_obj(dev_name);

    while (!end_iter) {
        cam_params = at_obj.GetNextParams();
        cv::Mat img = cap_obj.CapImg(cam_params);
        end_iter = at_obj.Run(img);
        cv::imwrite("/tmp/at_res/" + std::to_string(iter) + ".png", img);
        iter += 1;
    }

    // 6. Get final image and print best parameters
    cam_params = at_obj.GetBestParams();
    ar_params = at_obj.GetARParams();
    cv::Mat final_img = cap_obj.CapImg(cam_params);
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
        printf("Please pass the device name as the first argument.\n");
        return 0;
    }

    if (argc == 2) {
        std::string dev_name = argv[1];
        TestATOnline(dev_name);
    }

    return 0;
}
