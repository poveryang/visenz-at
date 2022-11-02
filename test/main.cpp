#include <opencv2/core.hpp>

#include "smore_barcode.h"
#include "at_interface.h"
#include "at_params.h"
#include "cap_image.h"


void TestATOnline()
{
    // 1. Set device name
    std::string dev_name = "VS1000P";

    // 2. Get the initial params from UI
    at::CamParams cam_params;
    cam_params.lights = {12, 12, 12, 12};
    cam_params.exp_time = 150;
    cam_params.exp_gain = 30;
    cam_params.focus_pos = 60;
    cam_params.roi = {0, 0, 1280, 800};

    // 3. Set AR parameters and Barcode wrapper
    at::ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // 4. Initialization of AT object
    at::ATInterface at_obj(dev_name, cam_params, barcode_wrapper);
    std::string at_version = at_obj.GetVersion();
    printf("AT Version: %s \n", at_version.c_str());

    bool enable_al = true;
    bool enable_af = true;
    bool enable_ae = true;
    bool enable_ar = true;
    at_obj.Init(enable_al, enable_af, enable_ae, enable_ar);

    InitCap(cam_params);
    std::cout << "\n\n>>>>>===== AT has been started <<<<<=====\n\n" << std::endl;

    // 5. This is the main loop of the AT algorithm
    bool end_iter = false;
    int iter = 0;
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
//    ar_params.print();

    std::cout << ">>>>>===== AT has been ended <<<<<=====\n\n " << std::endl;
}

int main() {
    TestATOnline();
    return 0;
}