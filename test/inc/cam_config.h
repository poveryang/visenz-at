#ifndef CAM_CONFIG_H
#define CAM_CONFIG_H
#include "at_interface.h"

namespace at{
    CamConf vs1000p_conf = {
            .img_width = 1280,
            .img_height = 800,
            .roi = cv::Rect2d(0, 0, 0, 0),
            .max_lt = 2000,
            .min_intensity = 1,
            .max_intensity = 24,
            .min_et = 20,
            .max_et = 39000,
            .min_eg = 16,
            .max_eg = 255,
            .eg_quant_scale = 0.0625,
            .lens_type = 0,
            .start_pos = 0,
            .end_pos = 410,
            .ae_mode = 0,
            .init_et = 1000,
            .init_eg = 16,
            .init_intensities = {24, 24, 24, 24},
            .init_pos = 60,
    };
}

#endif //CAM_CONFIG_H
