#ifndef AT_BARCODE_BASE_H
#define AT_BARCODE_BASE_H

#include "barcode.h"
#include <opencv2/opencv.hpp>


/** A struct that is used to store the AR parameters.*/
struct ARInfo {
    // float ppm = 0;  // Barcode minimal ppm
    // std::array<int, 2> version = {-1, -1};  // For 2d barcode only
    // int number_1D = 0;
    // int number_2D = 0;
    // bool read_1D = false;
    // bool read_2D = false;
    // int polarity_1D = 0;
    // int polarity_2D = 0;
    // int mirror_2D = 0;
    // int DM_2D = 2; //0 : 正方形 1 ：长方形 2 ：兼容模式
    // bool codelist[12] = {false};
    // cv::Rect rect;

    std::vector<smartmore::barcode::BarcodeType> successful_code_type;
    std::array<int, 2> qr_version = {-1, -1};
    std::array<int, 2> dm_version = {-1, -1};

    void reset() {
        // number_1D = 0;
        // number_2D = 0;
        // read_1D = false;
        // read_2D = false;
        // DM_2D = 2;
        // polarity_1D = 0;
        // polarity_2D = 0;
        // mirror_2D = 0;
        // std::fill(codelist, codelist + 12, false);
        // rect = cv::Rect(0, 0, image.cols, image.rows);

        this->successful_code_type.clear();
        std::array<int, 2> qr_version = {-1, -1};
        std::array<int, 2> dm_version = {-1, -1};
    }

    void print()
    {
        // printf("number_1D : %d\n", number_1D);
        // printf("number_2D : %d\n", number_2D);
        // printf("polarity_1D : %d\n", polarity_1D);
        // printf("polarity_2D : %d\n", polarity_2D);
        // printf("DM_2D : %d\n", DM_2D);
        // printf("mirror_2D : %d\n", mirror_2D);
        // for (int i = 1; i < sizeof(codelist); i++)
        // {
        //     printf("codelist[%d] : %d\n",i , codelist[i]);
        // }

        std::cout << "qr version: " << this->qr_version[0] << std::endl;
        std::cout << "dm version: [" << this->dm_version[0] << ", " << this->dm_version[1] << "]" << std::endl;
    }
};

/** @brief This is a virtual base class of Barcode Wrapper.*/
class BarcodeWrapperBase {
public:
    virtual void Reset() = 0;

    virtual std::vector<cv::Rect> Decode(const cv::Mat &image, ARInfo &out_arinfo) = 0;

    virtual void SetOriginParams() = 0;
};

/** @brief Class of AR Interface.*/
// class ARInterface {
// public:
//     ARInterface() = default;

//     ~ARInterface() = default;

//     void Init(BarcodeWrapperBase &barcode_wrapper) {
//         barcode_wrapper_ = &barcode_wrapper;
//         barcode_wrapper_->Reset();
//     };

//     void Decode(const cv::Mat &image, ARParams &ar_params) {
//         barcode_wrapper_->Decode(image);
//     };

//     void ResetOriParams() {
//         barcode_wrapper_->SetOriginParams();
//     }

// private:
//     BarcodeWrapperBase *barcode_wrapper_{};
// };

#endif //AT_BARCODE_BASE_H
