#include <opencv2/core.hpp>

#include "cap_image.h"

#include "AT_interface.h"
#include "at_config.h"

void SaveImg(const cv::Mat& img, int iter, const std::string& phase, const std::string& save_dir="/tmp/at_res/");

void TestATOnline()
{
    // 1. Instantiate AT object and AT config
    at::ATConfig at_config;
    at::ATInterface at_obj;

    // 2. Set device name
    std::string dev_name = "VS1000PRO";
    at_obj.setDevice(dev_name);

    // 3. Initialization of AT object
    bool enable_al = true;
    bool enable_af = true;
    bool enable_ae = true;
    bool enable_ar = true;
    int time = 0;  // This parameter will not be used
    at_obj.init(enable_ae, enable_af, enable_al, enable_ar, time);

    // 4. Get the initial params from UI and init hardware
    at_config.hardware.lights = {0, 0, 0, 0};
    at_config.hardware.motor = 61;
    at_config.hardware.exposure = 150;
    at_config.hardware.gain = 30;
    at_config.hardware.roi = {0, 0, 1280, 800};
    at_obj.init_hardware(at_config);

    // 4. Get the AE configuration from UI and init ae
    if (enable_ae){
        int mode = 0;  // Execution mode, 0/shutter priority, 1/gain priority, 2/comprehensive mode
        int min_exposure = 20;  // Configurable minimum exposure time
        int max_exposure = 10000;  // Configurable maximum exposure time
        int min_gain = 1;  // Configurable minimum exposure gain
        int max_gain = 255;  // Configurable maximum exposure gain
        at_obj.init_ae(mode, min_exposure, max_exposure, min_gain, max_gain);
    }

    // 5. Load barcode sdk
    smartmore::barcode::Barcode sdk("/usr/scanner/algorithm/");
    sdk.LoadConfig("/usr/scanner/algorithm/config_dl.json");
    at_obj.setMetrics_Barcode(sdk);

    /* This is the main loop of the AT algorithm.
     * Capturing images from the camera, running the AT algorithm, and saving the images. */
    cv::Mat img;
    InitCap(at_config.hardware);
    std::cout << "\n\n>>>>>===== AT has been started <<<<<=====\n\n" << std::endl;
    int iter = 0;
    std::vector<std::string> at_phases = {"AL", "AF", "AE", "AR", "END"};
    while (!at_obj.m_config._end_iteration) {
        img = ATCapImg(at_obj.m_config.hardware);
        at_obj.run(img);

        if (at_obj.cur_phase != at::END){
            cv::rectangle(img, at_obj.m_config.hardware.roi, 255, 3);
            SaveImg(img, iter, at_phases[at_obj.cur_phase]);
            iter += 1;
        }
    }
    at_obj.m_config.hardware.lights = at_obj.m_config.best_lights;
    at_obj.m_config.hardware.exposure = at_obj.m_config.best_exposure;
    at_obj.m_config.hardware.gain = at_obj.m_config.best_gain;
    at_obj.m_config.hardware.motor = at_obj.m_config.best_motor;
    cv::Mat final_img = ATCapImg(at_obj.m_config.hardware);
    SaveImg(final_img, iter+1, "FINAL");
    std::cout << ">>>>>===== AT has been ended <<<<<=====\n\n " << std::endl;
}

void SaveImg(const cv::Mat& img, int iter, const std::string& phase, const std::string& save_dir){
    V4L2Capture& vcap = V4L2Capture::getInstance();
    sensorParam params = vcap.getCurrentFrameSensorParam();
    uint lights = params.lightBright[0];
    uint exp_time = params.exposure;
    uint exp_gain = params.gain;
    uint focus_pos = params.focus;

    std::stringstream file_name;
    file_name << save_dir << iter << "_" << phase
              << "_l" << lights
              << "_t" << exp_time
              << "_g" << exp_gain
              << "_f" << focus_pos
              << ".png";

    cv::imwrite(file_name.str(), img);
}

int main() {
    TestATOnline();
    return 0;
}