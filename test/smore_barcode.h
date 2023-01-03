#ifndef SM_BARCODE_WRAPPER_H
#define SM_BARCODE_WRAPPER_H

#include "barcode.h"
#include "at_interface.h"

struct BarcodeParams {
    smartmore::barcode::BarcodeType barcode_type;
    int dl_nums_instance;
    // 1D code params
    smartmore::barcode::Polarity polarity_1d;
    smartmore::barcode::RunningMode running_mode_1d;
    int nums_max_output_1d;
    // 2D code params
    smartmore::barcode::Polarity polarity_2d;
    smartmore::barcode::MirrorType mirror_type_2d;
    smartmore::barcode::RunningMode running_mode_2d;
    smartmore::barcode::QrDistortionType qe_distortion_type;
    smartmore::barcode::DMEdgeType dm_edge_type;
    smartmore::barcode::DMShapeType dm_shape_type;
    bool check_digit_enable_code39;
    int nums_max_output_2d;
};

class BarcodeWrapper : public at::BarcodeWrapperBase {
private:
    smartmore::barcode::Barcode *barcode_sdk_;
    BarcodeParams origin_params;

public:
    explicit BarcodeWrapper(smartmore::barcode::Barcode &barcode_sdk) {
        barcode_sdk_ = &barcode_sdk;
    }

    void SetOriginParams() override {
        barcode_sdk_->SetBarcodeType(origin_params.barcode_type);
        barcode_sdk_->SetDLNumsInstance(origin_params.dl_nums_instance);
        // Set origin 1D code params
        barcode_sdk_->SetPolarity1D(origin_params.polarity_1d);
        barcode_sdk_->SetRunningMode1D(origin_params.running_mode_1d);
        barcode_sdk_->SetNumsMaxOutput1D(origin_params.nums_max_output_1d);
        // Set origin 2D code params
        barcode_sdk_->SetPolarity2D(origin_params.polarity_2d);
        barcode_sdk_->SetMirrorType2D(origin_params.mirror_type_2d);
        barcode_sdk_->SetRunningMode2D(origin_params.running_mode_2d);
        barcode_sdk_->SetQrDistortionType(origin_params.qe_distortion_type);
        barcode_sdk_->SetDmEdgeType(origin_params.dm_edge_type);
        barcode_sdk_->SetDmShapeType(origin_params.dm_shape_type);
        barcode_sdk_->SetCheckDigitEnableCode39(origin_params.check_digit_enable_code39);
        barcode_sdk_->SetNumsMaxOutput2D(origin_params.nums_max_output_2d);
    };

    void Reset() override {
        // Get origin parameters
        origin_params.barcode_type = barcode_sdk_->GetBarcodeType();
        origin_params.dl_nums_instance = barcode_sdk_->GetDLNumsInstance();
        // Get origin 1D code params
        origin_params.polarity_1d = barcode_sdk_->GetPolarity1D();
        origin_params.running_mode_1d = barcode_sdk_->GetRunningMode1D();
        origin_params.nums_max_output_1d = barcode_sdk_->GetNumsMaxOutput1D();
        // Get origin 2D code params
        origin_params.polarity_2d = barcode_sdk_->GetPolarity2D();
        origin_params.mirror_type_2d = barcode_sdk_->GetMirrorType2D();
        origin_params.running_mode_2d = barcode_sdk_->GetRunningMode2D();
        origin_params.qe_distortion_type = barcode_sdk_->GetQrDistortionType();
        origin_params.dm_edge_type = barcode_sdk_->GetDmEdgeType();
        origin_params.dm_shape_type = barcode_sdk_->GetDmShapeType();
        origin_params.check_digit_enable_code39 = barcode_sdk_->GetCheckDigitEnableCode39();
        origin_params.nums_max_output_2d = barcode_sdk_->GetNumsMaxOutput2D();

        // Reset Parameters
        barcode_sdk_->SetBarcodeType(smartmore::barcode::BarcodeType::kUnknown);
        barcode_sdk_->SetDLNumsInstance(200);
        // Reset 1D code params
        barcode_sdk_->SetPolarity1D(smartmore::barcode::Polarity::kAuto);
        barcode_sdk_->SetRunningMode1D(smartmore::barcode::RunningMode::kAuto);
        barcode_sdk_->SetNumsMaxOutput1D(200);
        // Reset 2D code params
        barcode_sdk_->SetPolarity2D(smartmore::barcode::Polarity::kAuto);
        barcode_sdk_->SetMirrorType2D(smartmore::barcode::MirrorType::kAuto);
        barcode_sdk_->SetRunningMode2D(smartmore::barcode::RunningMode::kAuto);
        barcode_sdk_->SetQrDistortionType(smartmore::barcode::QrDistortionType::kAuto);
        barcode_sdk_->SetDmEdgeType(smartmore::barcode::DMEdgeType::kAuto);
        barcode_sdk_->SetDmShapeType(smartmore::barcode::DMShapeType::kAuto);
        barcode_sdk_->SetCheckDigitEnableCode39(false);
        barcode_sdk_->SetNumsMaxOutput2D(200);
    };

    std::vector <cv::Rect> Decode(const cv::Mat &image, at::ARParams &ar_params) override {
        smartmore::barcode::BarcodeRequest input = {image};
        smartmore::barcode::BarcodeResponse output;
        barcode_sdk_->Run(input, output);

        std::vector <cv::Rect> rects;
        ar_params.reset(image);
        bool first_1D_read = false;
        bool first_2D_read = false;
        bool first_DM_read = false;

        if (!output.results.empty()) {
            auto cur_count = output.results.size();
            for (int i = 0; i < cur_count; i++) {
                smartmore::barcode::BarcodeInfo result = output.results[i];
                ar_params.ppm = result.ppm;
                ar_params.version = result.version;

                // 没有解到码也有信息输出，也会返回results，这里需要判断是否解码成功
                if (!result.succeed) { continue; }
                int top = image.rows;
                int left = image.cols;
                int bottom = 0;
                int right = 0;
                for (int point_i = 0; point_i < 4; point_i++) {
                    top = std::min(top, int(result.corner_boxes[point_i].y));
                    bottom = std::max(bottom, int(result.corner_boxes[point_i].y));

                    left = std::min(left, int(result.corner_boxes[point_i].x));
                    right = std::max(right, int(result.corner_boxes[point_i].x));
                }
                top = std::max(0, top);
                left = std::max(0, left);
                right = std::min(image.cols, right);
                bottom = std::min(image.rows, bottom);
                cv::Rect rect = cv::Rect(left, top, std::max(0, right - left), std::max(0, bottom - top));
                rects.push_back(rect);

                //当前码是一维码还是二维码
                bool read_1d = false;
                bool read_2d = false;
                bool read_dm = false;
                if (result.type == smartmore::barcode::BarcodeType::kCode39) {
                    ar_params.number_1D++;
                    ar_params.codelist[1] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kCode93) {
                    ar_params.number_1D++;
                    ar_params.codelist[2] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kCode128) {
                    ar_params.number_1D++;
                    ar_params.codelist[3] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kEan8) {
                    ar_params.number_1D++;
                    ar_params.codelist[4] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kEan13) {
                    ar_params.number_1D++;
                    ar_params.codelist[5] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kItf) {
                    ar_params.number_1D++;
                    ar_params.codelist[6] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kUpcA) {
                    ar_params.number_1D++;
                    ar_params.codelist[7] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kUpcE) {
                    ar_params.number_1D++;
                    ar_params.codelist[8] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kUpcEanExtension) {
                    ar_params.number_1D++;
                    ar_params.codelist[9] = true;
                    read_1d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kQrcode) {
                    ar_params.number_2D++;
                    ar_params.codelist[10] = true;
                    read_2d = true;
                }
                if (result.type == smartmore::barcode::BarcodeType::kDmcode) {
                    ar_params.number_2D++;
                    ar_params.codelist[11] = true;
                    read_2d = true;
                    read_dm = true;
                }
                //是否读到一维码
                for (int index_1d = 1; index_1d <= 9; index_1d++) {
                    ar_params.read_1D = ar_params.codelist[index_1d] | ar_params.read_1D;
                }
                //一维码极性
                if (!first_1D_read && read_1d) {
                    ar_params.polarity_1D = static_cast<int>(result.polarity);
                    first_1D_read = true;
                } else {
                    if (ar_params.polarity_1D != static_cast<int>(result.polarity) && read_1d) {
                        ar_params.polarity_1D = static_cast<int>(smartmore::barcode::Polarity::kAuto);
                    }
                }
                //是否读到二维码
                ar_params.read_2D = ar_params.codelist[10] | ar_params.codelist[11];
                //二维码极性，镜像
                if (!first_2D_read && read_2d) {
                    ar_params.mirror_2D = static_cast<int>(result.mirror);
                    ar_params.polarity_2D = static_cast<int>(result.polarity);
                    first_2D_read = true;
                } else {
                    if (ar_params.polarity_2D != static_cast<int>(result.polarity) && read_2d) {
                        ar_params.polarity_2D = static_cast<int>(smartmore::barcode::Polarity::kAuto);
                    }
                    if (ar_params.mirror_2D != static_cast<int>(result.mirror) && read_2d) {
                        ar_params.mirror_2D = static_cast<int>(smartmore::barcode::MirrorType::kAuto);
                    }
                }
                //DM 码类型
                if (!first_DM_read && read_dm) {
                    //第一次读到DM码
                    //0 正方形 1 长方形 2 兼容模式
                    if (result.version[0] == result.version[1])
                        ar_params.DM_2D = 0;
                    else
                        ar_params.DM_2D = 1;
                    first_DM_read = true;
                } else {
                    int dm_result = 0;
                    if (result.version[0] != result.version[1]) {
                        dm_result = 1;
                    }
                    if (ar_params.DM_2D != dm_result && read_dm) {
                        ar_params.DM_2D = 2;
                    }
                }
            }
        }

        return rects;
    };

};

#endif //SM_BARCODE_WRAPPER_H