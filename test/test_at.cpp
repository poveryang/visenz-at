#include "at_interface.h"
#include "smore_barcode.h"
#include "driver_cap.h"
#include "cam_config.h"

std::map<std::string, CamConf> cam_conf_map = {
        {"vs1000p", at::vs1000p_conf},
};

void TestATOnline(const std::string& dev_name) {
    // 1. Prepare camera parameters
    CamConf cam_conf = cam_conf_map[dev_name];
    cam_conf.init_intensities = {24, 24, 24, 24};
//    cam_conf.roi.width = 1280;
//    cam_conf.roi.height = 800;

    // 2. Set Barcode wrapper and ar_params
    smartmore::barcode::Barcode barcode_sdk("/usr/scanner/algorithm/");
    barcode_sdk.LoadConfig("/usr/scanner/algorithm/config.json");
    BarcodeWrapper barcode_wrapper(barcode_sdk);
    ARParams ar_params;

    // 3. Initialize AT algorithm
    bool enable_hmap = true;
    at::ATInterface at_obj(enable_hmap);

    // 4. Set enable flags of the four algorithms
    bool enable_al, enable_af, enable_ae, enable_ar;
    enable_al = true;
    enable_af = true;
    enable_ae = true;
    enable_ar = false;
    at_obj.Init(cam_conf, barcode_wrapper,
            enable_al, enable_af, enable_ae, enable_ar);

    // 5. Execute AT algorithm
    CamParams cam_params = at_obj.GetNextParams();
    InitCap(cam_conf, cam_params);  // initialize camera capture
    cv::Mat tmp_img = CapImg(cam_params);  // capture image
    printf(">>>>>===== AT (version: %s) has been started <<<<<=====\n", at::ATInterface::GetVersion().c_str());

    int iter = -1;
    bool end_iter = false;
    while (!end_iter) {
        // get next camera parameters and capture image
        iter += 1;
        cam_params = at_obj.GetNextParams();
        cv::Mat img = CapImg(cam_params);
        std::string filename = "/tmp/at_res/" + std::to_string(iter) + "-" + cam_params.Str() + ".png";
        cv::imwrite(filename, img);

        // run AT
        end_iter = at_obj.Run(img);
    }
    printf(">>>>>===== AT has been ended <<<<<=====\n\n");

    /* get the best camera parameters */
    cam_params = at_obj.GetBestParams();
    printf("Best camera parameters:\n");
    cam_params.Print();

    /* get AR parameters */
    ar_params = at_obj.GetARParams();
    ar_params.print();

    /* get final image and save it */
    cv::Mat final_img = CapImg(cam_params);
    std::string filename = "/tmp/at_res/final-" + cam_params.Str() + ".png";
    cv::imwrite(filename, final_img);
    CloseCap();  // close camera capture
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
