#ifndef AT_INTERFACE_H
#define AT_INTERFACE_H

#include "at_config.h"

namespace at {
    class ATInterface {
    public:
        explicit ATInterface();

        ~ATInterface() = default;

        /**
         * The is the main function of the ATInterface class. It is called to run the AT algorithm
         *
         * @param image the input image
         */
        void Run(const cv::Mat &image);

        /**
         * It sets the device name, camera configuration, and load init camera parameters.
         *
         * @param dev_name The name of the device.
         * @param init_params The initial camera parameters.
         */
        void SetDevice(std::string &dev_name, CamParams init_params);

        /**
         * It initializes the ATInterface class.
         *
         * @param enable_ae Enable/Disable Auto Exposure
         * @param enable_af Enable/Disable Auto Focus
         * @param enable_al Enable/Disable Auto Light
         * @param enable_ar Enable/Disable Auto recognition
         */
        void Init(bool enable_ae, bool enable_af, bool enable_al, bool enable_ar);

        bool EndIter();

        CamParams GetNextParams();

        CamParams GetBestParams();

    private:
        // Flag to indicate whether the current iteration is the last iteration.
        bool end_iter_;

        // Variable that is used to keep track of the current phase of the Auto Tuning process.
        ATPhase cur_phase_;

        // Flag to indicate whether the corresponding module is enabled.
        bool en_al_, en_af_, en_ae_, en_ar_;

        // Variable that stores the camera configuration.
        CamConf cam_conf_;

        // Variable that stores the next camera parameters.
        CamParams next_params_;

        // Variable that stores the best camera parameters.
        CamParams best_params_;

        /** The concrete implementation class of the AT object.*/
        class ATImpl;
        std::shared_ptr<ATImpl> at_impl_;

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
