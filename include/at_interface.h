#ifndef AT_INTERFACE_H
#define AT_INTERFACE_H

#include <opencv2/opencv.hpp>
#include "at_params.h"

namespace at {
    /** @brief Defining the different phases of the Auto Tuning process.*/
    enum ATPhase {
        AL,
        AE4AF,
        AF,
        AE,
        AR,
        END,
    };

    /** @brief This structure stores the camera configuration.*/
    struct CamConf {
        /** Image Resolution*/
        int IMG_WIDTH;  // image width
        int IMG_HEIGHT;  // image height

        /** Fill Light control*/
        int MIN_INTENSITY;  // minimum light intensity
        int MAX_INTENSITY;  // maximum light intensity

        /** Exposure control*/
        int MIN_ET;  // minimum exposure time
        int MAX_ET;  // maximum exposure time
        int MIN_EG;  // minimum exposure gain
        int MAX_EG;  // maximum exposure gain


        /** Focus control*/
        int LENS_TYPE;  // 0: mechanical lens, 1: liquid lens
        int START_POS;  // start position of lens
        int END_POS; // end position of lens

        /** Initial camera parameters*/
        cv::Rect2i ROI;  // roi of the image
        std::vector<int> INIT_INTENSITIES;  // initial light intensities
        int AE_MODE;  // 0:auto, 1: shutter priority, 2: gain priority
        int INIT_ET;  // initial exposure time
        int INIT_EG;  // initial exposure gain
        int INIT_POS;  // initial position of lens
    };

    /** @brief This is a virtual base class of Barcode Wrapper.*/
    class BarcodeWrapperBase {
    public:
        virtual void SetOriginParams() = 0;

        virtual void Reset() = 0;

        virtual std::vector<cv::Rect> Decode(const cv::Mat &image, at::ARParams &ar_params) = 0;
    };

    class ATInterface {
    public:
        /**
         * The constructor of the ATInterface for VS series.
         *
         * @param cam_conf Camera configuration
         * @param barcode_wrapper The wrapper of the Barcode SDK
         */
        ATInterface(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper);

        /**
         * The initial function to enable/disable the corresponding module for VS series.
         *
         * @param enable_al Enable/Disable Auto Light
         * @param enable_af Enable/Disable Auto Focus
         * @param enable_ae Enable/Disable Auto Exposure
         * @param enable_ar Enable/Disable Auto recognition
         */
        void Init(bool enable_al, bool enable_af, bool enable_ae, bool enable_ar);

        /**
         * The constructor of the ATInterface for VN series.
         *
         * @param cam_conf Camera configuration
         */
        explicit ATInterface(CamConf &cam_conf);

        /**
         * The initial function to enable/disable the corresponding module for VN series.
         *
         * @param enable_al Enable/Disable Auto Light
         * @param enable_af Enable/Disable Auto Focus
         * @param enable_ae Enable/Disable Auto Exposure
         */
        void Init(bool enable_al, bool enable_af, bool enable_ae);

        /**
         * The main function to run the Auto Tuning process.
         *
         * @param image the input image
         * @return true if the Auto Tuning process is finished, otherwise false.
         */
        bool Run(const cv::Mat &image);

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

        /**
         * Used to get the version of the AT.
         * @return the version of the AT
         */
        static std::string GetVersion();

        ~ATInterface() = default;

    private:
        /** The concrete implementation class of the AT object.*/
        class ATImpl;

        std::shared_ptr<ATImpl> at_impl_;

        // Store the ROI of the image.
        cv::Rect2i image_roi_;

        std::vector<ATPhase> pipeline_;

        // Variable that is used to keep track of the current phase of the Auto Tuning process.
        std::vector<ATPhase>::iterator cur_phase_;

        // Flag to indicate whether the corresponding module is enabled.
        bool en_al_, en_af_, en_ae_, en_ar_;

        // Variable that stores the camera configuration.
        CamConf cam_conf_{};

        // Variable that stores the next camera parameters.
        CamParams next_params_;

        // Variable that stores the best camera parameters.
        CamParams best_params_;

        // Variable that stores the parameters of the AR module.
        ARParams ar_params_;

        double score = 0;

        std::map<double, CamParams> score_params_;

    private:
        /**
         * The main function to execute all the phases sequentially.
         */
        void SequentialExec(const cv::Mat &image);

        /**
         * Update the camera parameters
         */
        void UpdateNextParams();

        /**
         * Set the initial camera parameters.
         */
        void SetInitParams(CamConf &cam_conf);
    };
}

#endif //AT_INTERFACE_H
