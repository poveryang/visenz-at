#ifndef AT_PARAMS_H
#define AT_PARAMS_H

/** @brief This structure stores the camera configuration.*/
struct CamConf {
    /** Image Resolution*/
    int img_width;      // image width
    int img_height;     // image height
    cv::Rect2d roi;                     // roi of the image

    /** Fill Light control*/
    int max_lt;                     // maximum light time
    int min_intensity;              // minimum light intensity
    int max_intensity;              // maximum light intensity

    /** Exposure control*/
    int min_et;                 // minimum exposure time
    int max_et;                 // maximum exposure time
    int min_eg;                 // minimum exposure gain
    int max_eg;                 // maximum exposure gain
    double eg_quant_scale;      // quantization scale of exposure gain

    /** Focus control*/
    int lens_type;      // 0: mechanical lens, 1: liquid lens
    int start_pos;      // start position of lens
    int end_pos;        // end position of lens

    /** Initial camera parameters*/
    int ae_mode;                        // 0:auto, 1: shutter priority, 2: gain priority
    int init_et;                        // initial exposure time
    int init_eg;                        // initial exposure gain
    std::vector<int> init_intensities;  // initial light intensities
    int init_pos;                       // initial position of lens
};

/** @brief This structure stores the camera parameters.*/
struct CamParams {
    int exp_time; // exposure time
    int exp_gain; // exposure gain
    std::vector<int> lights;  // light intensities
    int focus_pos;  // focus position

    void Print() const {
        std::cout << "exp_time=" << exp_time << ", exp_gain= " << exp_gain << ", lights=(";
        for (auto &light : lights) {
            std::cout << light << ",";
        }
        std::cout << "), focus_pos=" << focus_pos << std::endl;
    }
};


#endif  // AT_PARAMS_H
