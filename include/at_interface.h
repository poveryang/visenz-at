#ifndef AT_INTERFACE_H
#define AT_INTERFACE_H

#include <opencv2/opencv.hpp>

#include "ar_base.h"
#include "at_base.h"

namespace at {
    class ATInterface {
    public:
        /**
         *  The default constructor of the ATInterface (For VN/VA).
         */
        ATInterface() = default;

        /** Initialize the ATInterface (For VN/VA)
         * @param cam_conf the camera configuration
         * @param en_al flag to indicate whether the AL is used
         * @param en_af flag to indicate whether the AF is used
         * @param en_ae flag to indicate whether the AE is used
         */
        void Init(CamConf &cam_conf, bool en_al, bool en_af, bool en_ae);

        /**
         *  The constructor of the ATInterface (For VS).
         *  @param en_hmap flag to indicate whether the heatmap-generator is used
         */
        explicit ATInterface(bool en_hmap);

        /**
         * Initialize the ATInterface (For VS)
         * @param cam_conf the camera configuration
         * @param barcode_wrapper the barcode wrapper
         * @param en_al flag to indicate whether the AL is used
         * @param en_af flag to indicate whether the AF is used
         * @param en_ae flag to indicate whether the AE is used
         * @param en_ar flag to indicate whether the AR is used
         */
        void Init(CamConf &cam_conf, BarcodeWrapperBase &barcode_wrapper,
                  bool en_al, bool en_af, bool en_ae, bool en_ar);

        /**
         * The main function to run the Auto Tuning process.
         *
         * @param image the input image
         * @return true if the Auto Tuning process is finished, otherwise false.
         */
        bool Run(const cv::Mat &image);

        /**
         * Get the next camera parameters.
         *
         * @return next camera parameters.
         */
        CamParams GetNextParams();

        /**
         * Get the best camera parameters.
         *
         * @return best camera parameters.
         */
        CamParams GetBestParams();

        /**
         * Get the best camera parameters.
         *
         * @return best camera parameters.
         */
        ARParams GetARParams();

        /**
         * Get the version of the AT.
         * @return the version of the AT
         */
        static std::string GetVersion();

        /** The destructor of the ATInterface.*/
        ~ATInterface() = default;

    private:
        /** The concrete implementation class of the AT object.*/
        std::shared_ptr<ATImplBase> at_impl_;
    };
}

#endif //AT_INTERFACE_H
