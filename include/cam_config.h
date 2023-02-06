#ifndef AT_CONFIG_H
#define AT_CONFIG_H

namespace at{
    CamConf vs1000p_conf = {
            .IMG_WIDTH = 1280,
            .IMG_HEIGHT = 800,
            .MIN_INTENSITY = 1,
            .MAX_INTENSITY = 24,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 0,
            .START_POS = 0,
            .END_POS = 410,
    };

    CamConf vs1000p2m_conf = {
            .IMG_WIDTH = 1920,
            .IMG_HEIGHT = 1200,
            .MIN_INTENSITY = 1,
            .MAX_INTENSITY = 24,
            .MIN_ET = 20,
            .MAX_ET = 1000000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 0,
            .START_POS = 0,
            .END_POS = 410,
    };

    CamConf vs800_conf = {
            .IMG_WIDTH = 1280,
            .IMG_HEIGHT = 800,
            .MIN_INTENSITY = 0,
            .MAX_INTENSITY = 1,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 1,
            .START_POS = 0,
            .END_POS = 1023,
    };

    CamConf vs2000_conf = {
            .IMG_WIDTH = 2448,
            .IMG_HEIGHT = 2048,
            .MIN_INTENSITY = 1,
            .MAX_INTENSITY = 24,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 0,
            .START_POS = 0,
            .END_POS = 410,
    };

    CamConf vs2000_2_conf = {
            .IMG_WIDTH = 2448,
            .IMG_HEIGHT = 2048,
            .MIN_INTENSITY = 1,
            .MAX_INTENSITY = 24,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 1,
            .START_POS = 0,
            .END_POS = 1023,
    };

    CamConf vn800_conf = {
            .IMG_WIDTH = 1280,
            .IMG_HEIGHT = 800,
            .MIN_INTENSITY = 0,
            .MAX_INTENSITY = 1,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 1,
            .START_POS = 0,
            .END_POS = 1023,
    };

    CamConf vn1000p_conf = {
            .IMG_WIDTH = 1280,
            .IMG_HEIGHT = 800,
            .MIN_INTENSITY = 1,
            .MAX_INTENSITY = 24,
            .MIN_ET = 20,
            .MAX_ET = 10000,
            .MIN_EG = 1,
            .MAX_EG = 255,
            .LENS_TYPE = 0,
            .START_POS = 0,
            .END_POS = 410,
    };
}

#endif //AT_CONFIG_H
