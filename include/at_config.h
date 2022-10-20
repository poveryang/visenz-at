#ifndef AT_CONFIG_H
#define AT_CONFIG_H
#include "ar_params.h"

namespace at{
    /** @brief Defining the different phases of the Auto Tuning process.*/
    enum ATPhase{
        AL = 0,
        AF = 1,
        AE = 2,
        AR = 3,
        END = 4
    };

    /** @brief This is a struct that is used to store the camera parameters.*/
    struct CamParams {
        /** Light intensities*/
        std::vector<int> lights;
        /** Focus position*/
        int focus_pos;
        /** Exposure time*/
        int exposure_time;
        /** Exposure gain*/
        int exposure_gain;
    };

    /** @brief A struct that is used to store the camera parameters.*/
    struct CamConf {
        /** Configurable params of AL: minimum light intensity*/
        int MIN_INTENSITY;
        /** Configurable params of AL: maximum light intensity*/
        int MAX_INTENSITY;

        /** Configurable params of AE: minimum exposure time*/
        int MIN_ET;
        /** Configurable params of AE: maximum exposure time*/
        int MAX_ET;
        /** Configurable params of AE: minimum exposure gain*/
        int MIN_EG;
        /** Configurable params of AE: maximum exposure gain*/
        int MAX_EG;

        /** Configurable params of AF: start position of motor*/
        int START_POS;
        /** Configurable params of AF: end position of motor*/
        int END_POS;
    };

    /** A struct that is used to store the AR parameters.*/
    ar::ARParams ar_params;
}


#endif  // AT_CONFIG_H
