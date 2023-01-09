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
            .START_POS = 0,
            .END_POS = 410,
            .LENS_TYPE = "mechanical",
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
            .START_POS = 0,
            .END_POS = 410,
            .LENS_TYPE = "mechanical",
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
            .START_POS = 0,
            .END_POS = 1023,
            .LENS_TYPE = "liquid",
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
            .START_POS = 0,
            .END_POS = 410,
            .LENS_TYPE = "mechanical",
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
            .START_POS = 0,
            .END_POS = 1023,
            .LENS_TYPE = "liquid",
    };
}

#endif //AT_CONFIG_H
