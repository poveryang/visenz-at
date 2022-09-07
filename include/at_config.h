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

        /** Configurable params of AE: execute mode*/
        int MODE;
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
        /** Configurable params of AF: step size of motor*/
        int STEP_SIZE;
    };

    /** @brief This is a struct that is used to store the AT parameters.*/
    typedef struct ATParams
    {
    public:
        /** This is a flag that is used to stop the Auto Tuning process.*/
        bool _end_iteration= false;

        /** A struct that is used to store the AR parameters.*/
        ar::ARParams ar_config;

        /** This is a struct that is used to store the hardware parameters.*/
        struct HDParams
        {
            /** Light intensities:
             * The value range depends on different devices, such as VS1000P is 0-24*/
            std::vector<int> lights = {24, 24, 24, 24};

            /** Exposure time:
             * The value range depends on different devices, such as VS1000P is 0-10000*/
            int exposure = 1000;

            /** Exposure gain:
             * The value range depends on different devices, such as VS1000P is 1-255*/
            int gain = 30;

            /** Exposure gain:
             * The value range depends on different devices, such as VS1000P is 0-420*/
            int motor = 60;

            /** Region of Interest of the image.*/
            cv::Rect roi;
        };
        HDParams hardware;

        /** Best light intensities*/
        std::vector<int> best_lights = {24, 24, 24, 24};

        /** Best exposure time*/
        int best_exposure = 1000;

        /** Best exposure gain*/
        int best_gain = 30;

        /** Best motor position*/
        int best_motor = 60;

        /** This is a function that is used to print the AT parameters.*/
        void print()
        {
            std::cout << "best_gain : " << best_gain << std::endl;
            std::cout << "best_exposure : " << best_exposure << std::endl;
            std::cout << "best_motor : " << best_motor << std::endl;
            for (int best_light : best_lights)
                std::cout << "best_lights : " << best_light << std::endl;
            std::cout << "number_1D : " << ar_config.number_1D << std::endl;
            std::cout << "number_2D : " << ar_config.number_2D << std::endl;
            std::cout << "read_1D : " << ar_config.read_1D << std::endl;
            std::cout << "read_2D : " << ar_config.read_2D << std::endl;
            std::cout << "polarity_1D : " << int(ar_config.polarity_1D) << std::endl;
            std::cout << "polarity_2D : " << int(ar_config.polarity_2D) << std::endl;
            std::cout << "mirror_2D : " << int(ar_config.mirror_2D) << std::endl;
            std::cout << "DM_2D : " << ar_config.DM_2D << std::endl;
            for (int i = 1; i < sizeof(ar_config.codelist); i++)
            {
                std::cout << "codelist[" << i << "] : "<< ar_config.codelist[i] << std::endl;
            }
        }
    }ATConfig;
}


#endif  // AT_CONFIG_H
