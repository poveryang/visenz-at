#ifndef AT_INTERFACE_H
#define AT_INTERFACE_H

#include <opencv2/opencv.hpp>
#include "at_params.h"

namespace at {
    /** @brief Defining the different phases of the Auto Tuning process.*/
    enum ATPhase{
        AL = 0,
        AF = 1,
        AE = 2,
        AR = 3,
        END = 4
    };

    /** @brief A struct that is used to store the camera parameters.*/
    struct CamConf {
        /** Image Resolution: width*/
        int IMG_WIDTH;
        /** Image Resolution: height*/
        int IMG_HEIGHT;

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

    /** @brief This is a virtual base class of Barcode Wrapper.*/
    class BarcodeWrapperBase {
    public:
        virtual void Reset() = 0;

        virtual std::vector<cv::Rect> Decode(const cv::Mat &image, at::ARParams ar_params) = 0;
    };

    class ATInterface {
    public:
        ATInterface(std::string &dev_name, CamParams &init_params, BarcodeWrapperBase &barcode_wrapper);

        ~ATInterface() = default;

        /**
         * The is the main function of the ATInterface class. It is called to run the AT algorithm
         *
         * @param image the input image
         * @return whether the iteration is ended
         */
        bool Run(const cv::Mat &image);

        /**
         * It initializes the ATInterface class.
         *
         * @param enable_ae Enable/Disable Auto Exposure
         * @param enable_af Enable/Disable Auto Focus
         * @param enable_al Enable/Disable Auto Light
         * @param enable_ar Enable/Disable Auto recognition
         */
        void Init(bool enable_ae, bool enable_af, bool enable_al, bool enable_ar);

        /**
         * Used to get the next camera parameters.
         *
         * @return next camera parameters.
         */
        CamParams GetNextParams();

        /**
         * Used to get the best camera parameters.
         *
         * @return best camera parameters.
         */
        CamParams GetBestParams();

        /**
         * Used to get the best camera parameters.
         *
         * @return best camera parameters.
         */
        ARParams GetARParams();

    private:
        /** The concrete implementation class of the AT object.*/
        class ATImpl;
        std::shared_ptr<ATImpl> at_impl_;

        // Store the ROI of the image.
        cv::Rect2i image_roi_;

        // Variable that is used to keep track of the current phase of the Auto Tuning process.
        ATPhase cur_phase_;

        // Flag to indicate whether the corresponding module is enabled.
        bool en_al_, en_af_, en_ae_, en_ar_;

        // Variable that stores the camera configuration.
        CamConf cam_conf_{};

        // Variable that stores the next camera parameters.
        CamParams next_params_;

        // Variable that stores the best camera parameters.
        CamParams best_params_;

        ARParams ar_params_;

        BarcodeWrapperBase *barcode_wrapper_;

    private:
        /**
         * Update camera hardware parameters
         */
        void UpdateNextParams();

        /**
         * Update the current phase of the algorithm
         */
        void UpdateCurPhase();

        /**
         * It sets the camera device configuration, and loads init camera parameters.
         *
         * @param dev_name The name of the device.
         * @param init_params The initial camera parameters.
         */
        void SetDevice(std::string &dev_name, CamParams &init_params);

        void SequentialExec(const cv::Mat &image);
    };
}

#endif //AT_INTERFACE_H
