#ifndef SM_BARCODE_WRAPPER_H
#define SM_BARCODE_WRAPPER_H

#include "barcode.h"
#include "ar_base.h"

// struct BarcodeParams {
//     smartmore::barcode::BarcodeType barcode_type;
//     int dl_nums_instance;
//     // 1D code params
//     smartmore::barcode::Polarity polarity_1d;
//     smartmore::barcode::RunningMode running_mode_1d;
//     int nums_max_output_1d;
//     // 2D code params
//     smartmore::barcode::Polarity polarity_2d;
//     smartmore::barcode::MirrorType mirror_type_2d;
//     smartmore::barcode::RunningMode running_mode_2d;
//     smartmore::barcode::DMEdgeType dm_edge_type;
//     smartmore::barcode::DMShapeType dm_shape_type;
//     bool check_digit_enable_code39;
//     int nums_max_output_2d;
// };

class BarcodeWrapper : public BarcodeWrapperBase {
private:
    smartmore::barcode::Barcode *barcode_sdk_;
    // BarcodeParams origin_params;

public:
    explicit BarcodeWrapper(smartmore::barcode::Barcode &barcode_sdk) {
        barcode_sdk_ = &barcode_sdk;
    }

    void SetOriginParams() override {
    };

    void Reset() override {
    };

    // std::vector<cv::Rect> GetSdkRois()
    // {
    //     std::vector<cv::Rect> rois;
    //     auto sdk_rois = this->barcode_sdk_->GetRois();
    //     for(const auto &roi : sdk_rois)
    //     {
    //         rois.emplace_back(cv::boundingRect(roi));
    //     }

    //     return rois;
    // }

    std::vector <cv::Rect> Decode(const cv::Mat &image, ARInfo &out_arinfo) override {
        /*
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
        */
    
        smartmore::barcode::BarcodeRequest input = {image};
        smartmore::barcode::BarcodeResponse output;
        this->barcode_sdk_->Run(input, output);

        out_arinfo.reset();
        std::vector<std::array<int, 2>> qr_versions;
        std::vector<std::array<int, 2>> dm_versions;
        std::vector<cv::Rect> code_regions;  // 计算码区的清晰度，作为评判标准
        for(const auto &result : output.results)
        {
            if(result.detect_succeed)
            {
                // author:  lushaoan
                // comment: 这个判断看起来很多余，因为result.type是由sdk设置的时候就已经确定，看起来不会返回错误的类型
                //          但是，由于pdf417的存在，当sdk.SetBarcodeType(kCode2D)的情况下，会跑一维码的分类和精定位模型
                //          就会导致在result.detect_succeed的内容里会包含一维码的结果
                //          假如场景中同时有一维码和二维码，且两者需要的焦距是不一样的话，就会导致对焦错误，因为被一维码的定位结果影响了
                if(!smartmore::barcode::IsIntersect(result.type, this->barcode_sdk_->GetBarcodeType()))
                {
                    continue;
                }

                std::vector<cv::Point> pts = {
                    result.precise_locate_corner_boxes[0],
                    result.precise_locate_corner_boxes[1],
                    result.precise_locate_corner_boxes[2],
                    result.precise_locate_corner_boxes[3]};
                
                code_regions.emplace_back(cv::boundingRect(pts));
            }
            
            if(result.decode_succeed)
            {
                auto type_it = std::find(out_arinfo.successful_code_type.begin(), out_arinfo.successful_code_type.end(), result.type);
                if(type_it != out_arinfo.successful_code_type.end())
                {
                    out_arinfo.successful_code_type.emplace_back(result.type); // 记录解码成功的码制
                }

                if(result.type == smartmore::barcode::BarcodeType::kQrcode)
                {
                    auto ver_it = std::find(qr_versions.begin(), qr_versions.end(), result.version);
                    if(ver_it != qr_versions.end())
                    {
                        qr_versions.emplace_back(result.version);
                    }
                }
                else if(result.type == smartmore::barcode::BarcodeType::kDmcode)
                {
                    auto ver_it = std::find(dm_versions.begin(), dm_versions.end(), result.version);
                    if(ver_it != dm_versions.end())
                    {
                        dm_versions.emplace_back(result.version);
                    }
                }                
                
            }
        }

        // 若找到多个qr/dm版本，说明是多码，此时ar结果保持-1
        if(qr_versions.size() == 1)
        {
            out_arinfo.qr_version = qr_versions[0];
        }
        if(dm_versions.size() == 1)
        {
            out_arinfo.dm_version = dm_versions[0];
        }

        return code_regions;
    };

};

#endif //SM_BARCODE_WRAPPER_H