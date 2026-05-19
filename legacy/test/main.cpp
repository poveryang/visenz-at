#include "at_interface.h"
#include "cam_cap.h"
#include "smore_barcode.h"
#include "device_info.h"

int main() {
    // Initialize the camera capture
    CamCapture cam_cap;

    // Initialize the barcode wrapper
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    if (const auto it = BARCODE_CONFIG_SIGNATURES.find(DEVICE_NAME); it != BARCODE_CONFIG_SIGNATURES.end()) {
        barcode_sdk.SetConfigSignature(it->second);
    }
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // Initialize the AT interface
    at::ATInterface at_interface(false);
    CamConf cam_conf = DEVICE_MAP.at(DEVICE_NAME).GetCamConf();
    at_interface.Init(cam_conf, barcode_wrapper, false, true, true, false);

    // Execute the AT process
    bool is_finished = false;
    while (!is_finished) {
        auto [exp_time, exp_gain, lights, focus_pos] = at_interface.GetNextParams();
        cam_cap.SetCamParams(exp_time, exp_gain, focus_pos, lights);
        cv::Mat image = cam_cap.CapImg();
        is_finished = at_interface.Run(image);
    }

    // Get the best camera parameters and save the captured image
    auto [best_exp_time, best_exp_gain, best_lights, best_focus_pos] = at_interface.GetBestParams();
    cam_cap.SetCamParams(best_exp_time, best_exp_gain, best_focus_pos, best_lights);
    cv::Mat best_image = cam_cap.CapImg();
    cv::imwrite("best_image.png", best_image);

    return 0;
}