#include "smore_barcode.h"

#include "driver_cap.h"
#include "cam_config.h"


void copy_params(at::CamParams &src, DCapParams &dst) {
    dst.exp_time = src.exp_time;
    dst.exp_gain = src.exp_gain;
    dst.focus_pos = src.focus_pos;
    dst.lights = src.lights;
}


// Map of camera parameters
std::map<std::string, at::CamConf> cam_conf_map = {
        {"vs1000p", at::vs1000p_conf},
        {"vs800",   at::vs800_conf}
};


// Get Barcode wrapper
BarcodeWrapper GetBarcodeWrapper(const std::string &dev_name) {
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    if (dev_name == "vs1000p") {
        barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    }
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);
    return barcode_wrapper;
}


void TestATOnline(const std::string& dev_name) {
    DCapParams dcap_params;
    // 1. Prepare camera parameters
    at::CamConf cam_conf = cam_conf_map[dev_name];
    cam_conf.INIT_POS = 87;
    cam_conf.INIT_INTENSITIES = {1, 1, 1, 1};

    // 2. Set AR parameters and Barcode wrapper
    at::ARParams ar_params;
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
//    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config_dl_100w.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);

    // 3. Initialize AT algorithm
    at::ATInterface at_obj(cam_conf, barcode_wrapper);

    // 4. Set enable flags of the four algorithms
    bool enable_al, enable_af, enable_ae, enable_ar, enable_hmap;
    enable_al = true;
    enable_af = true;
    enable_ae = true;
    enable_ar = false;
    enable_hmap = true;
    at_obj.Init(enable_al, enable_af, enable_ae, enable_ar, enable_hmap);

    // 5. Execute AT algorithm
    std::string at_version = at::ATInterface::GetVersion();
    printf(">>>>>===== AT (version: %s) has been started <<<<<=====\n", at_version.c_str());

    int iter = 0;
    bool end_iter = false;
    at::CamParams cam_params = at_obj.GetNextParams();

    copy_params(cam_params, dcap_params);
    InitCap(dcap_params);

    while (!end_iter) {
        cam_params = at_obj.GetNextParams();
        copy_params(cam_params, dcap_params);

        printf("cap params: et = %d, eg = %d, pos = %d, lights = %d, %d, %d, %d\n",
               cam_params.exp_time, cam_params.exp_gain, cam_params.focus_pos,
               cam_params.lights[0], cam_params.lights[1], cam_params.lights[2], cam_params.lights[3]);
        cv::Mat img = CapImg(dcap_params);

        end_iter = at_obj.Run(img);
        // concatenate the camera parameters
        std::string cam_params_str = std::to_string(cam_params.exp_time) + "-" +
                                     std::to_string(cam_params.exp_gain) + "-" +
                                     std::to_string(cam_params.focus_pos) + "-" +
                                     std::to_string(cam_params.lights[0]) + "-" +
                                     std::to_string(cam_params.lights[1]) + "-" +
                                     std::to_string(cam_params.lights[2]) + "-" +
                                     std::to_string(cam_params.lights[3]);
        std::string img_name = "/tmp/at_res/" + std::to_string(iter) + "_" + cam_params_str + ".png";
        cv::imwrite(img_name, img);
        iter += 1;
        // wait for 10 ms
        usleep(10 * 1000);
    }

    cam_params = at_obj.GetBestParams();
    copy_params(cam_params, dcap_params);
    ar_params = at_obj.GetARParams();

    cv::Mat final_img = CapImg(dcap_params);
    cv::imwrite("/tmp/at_res/" + std::to_string(iter) + "_final.png", final_img);
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
