#include <opencv2/core.hpp>

#include "smore_barcode.h"
#include "at_interface.h"
#include "at_params.h"
#include "cap_image.h"
#include "cam_config.h"


// Map of camera parameters
std::map<std::string, at::CamConf> cam_conf_map = {
        {"vs1000p", at::vs1000p_conf},
        {"vs800", at::vs800_conf}
};


// Get Barcode wrapper
BarcodeWrapper GetBarcodeWrapper(const std::string& dev_name) {
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    if (dev_name == "vs1000p") {
        barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    }
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);
    return barcode_wrapper;
}


void TestATOnline(const std::string& dev_name) {
    // 1. Prepare camera parameters
    at::CamConf cam_conf = cam_conf_map[dev_name];

    // 2. Set AR parameters and Barcode wrapper
    at::ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // 3. Initialize AT algorithm
    at::ATInterface at_obj(cam_conf, barcode_wrapper);

    // 4. Set enable flags of the four algorithms
    bool enable_al, enable_af, enable_ae, enable_ar;
    enable_al = true;
    enable_af = true;
    enable_ae = true;
    enable_ar = false;
    at_obj.Init(enable_al, enable_af, enable_ae, enable_ar);

    // 5. Execute AT algorithm
    std::string at_version = at_obj.GetVersion();
    printf(">>>>>===== AT (version: %s) has been started <<<<<=====\n", at_version.c_str());

    int iter = 0;
    bool end_iter = false;
    at::CamParams cam_params = at_obj.GetNextParams();
    InitCap(cam_params);
    while (!end_iter) {
        cam_params = at_obj.GetNextParams();
        cv::Mat img = ATCapImg(cam_params);
        end_iter = at_obj.Run(img);
        cv::imwrite("/tmp/at_res/" + std::to_string(iter) + ".png", img);
        iter += 1;
    }

    cam_params = at_obj.GetBestParams();
    ar_params = at_obj.GetARParams();

    cv::Mat final_img = ATCapImg(cam_params);
    cv::imwrite("/tmp/at_res/" + std::to_string(iter) + ".png", final_img);
    ar_params.print();
    printf("best et = %d\n", cam_params.exp_time);
    printf("best eg = %d\n", cam_params.exp_gain);
    printf("best pos = %d\n", cam_params.focus_pos);
    printf("best lights = %d, %d, %d, %d\n",
           cam_params.lights[0], cam_params.lights[1], cam_params.lights[2], cam_params.lights[3]);

    printf(">>>>>===== AT has been ended <<<<<=====\n\n");
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Please pass the device name as the first argument.\n");
        return 0;
    }
    std::string dev_name = argv[1];
    TestATOnline(dev_name);
    return 0;
}
