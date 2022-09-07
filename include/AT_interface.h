#ifndef AT_INTERFACE_H
#define AT_INTERFACE_H

#include "at_config.h"

namespace at {
    class ATInterface {
    public:
        explicit ATInterface();

        ~ATInterface() = default;

        /**
         * It initializes the ATInterface class.
         *
         * @param enable_ae Enable/Disable Auto Exposure
         * @param enable_af Enable/Disable Auto Focus
         * @param enable_al Enable/Disable Auto Light
         * @param enable_ar Enable/Disable Auto recognition
         * @param time A deprecated interface parameter
         */
        void init(bool enable_ae, bool enable_af, bool enable_al, bool enable_ar, int time);

        /**
         * It sets the device name and the camera configuration
         *
         * @param dev_name The name of the device.
         */
        void setDevice(std::string &dev_name);

        /**
         * This function initializes the hardware configuration to the UI values
         *
         * @param hw_config the configuration of the hardware from UI
         */
        void init_hardware(at::ATConfig &hw_config);

        /**
         * It initializes the AE object.
         *
         * @param mode 0 for shutter priority, 1 for gain priority, 2 for comprehensive mode
         * @param min_et Minimum exposure time
         * @param max_et Maximum exposure time
         * @param min_eg Minimum exposure gain
         * @param max_eg Maximum exposure gain
         */
        void init_ae(int mode, int min_et, int max_et, int min_eg, int max_eg);

        /**
         * Sets the Barcode SDK to the AR object.
         *
         * @param barcode_sdk The barcode sdk object.
         */
        void setMetrics_Barcode(smartmore::barcode::Barcode &barcode_sdk);

        /**
         * The is the main function of the ATInterface class. It is called to run the AT algorithm
         *
         * @param image the input image
         */
        void run(const cv::Mat &image);

        /** @brief This is a flag to indicate whether the current iteration is the last iteration.*/
        bool end_iter;

        /** @brief Configuration object.*/
        ATConfig m_config;

        /** @brief This is a variable that is used to keep track of the current phase of the Auto Tuning process.*/
        ATPhase cur_phase;

    private:
        /** The concrete implementation class of the AT object.*/
        class ATImpl;
        std::shared_ptr<ATImpl> at_impl_{};

        /** A flag to indicate whether the corresponding module is enabled.*/
        bool en_al, en_af, en_ae, en_ar;

        /** The name of the device.*/
        std::string dev_name_{};

        /** Camera configuration.*/
        CamConf cam_conf_{};

        /**
         * Update camera hardware parameters
         */
        void UpdateNextParams();

        /**
         * Update the current phase of the algorithm
         */
        void UpdateCurPhase();
    };
}

#endif //AT_INTERFACE_H
