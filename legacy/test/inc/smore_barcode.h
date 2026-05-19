#ifndef SM_BARCODE_WRAPPER_H
#define SM_BARCODE_WRAPPER_H

#include "barcode.h"
#include "ar_base.h"

class BarcodeWrapper : public BarcodeWrapperBase {
private:
    smartmore::barcode::Barcode *barcode_sdk_;

public:
    explicit BarcodeWrapper(smartmore::barcode::Barcode &barcode_sdk) {
        barcode_sdk_ = &barcode_sdk;
    }

    void SetOriginParams() override {
    };

    void Reset() override {
    };

    std::vector <cv::Rect> Decode(const cv::Mat &image, ARInfo &out_arinfo, const cv::Rect &roi) override 
    {
        smartmore::barcode::BarcodeRequest input = {image};
        smartmore::barcode::BarcodeResponse output;
        auto sdk_rois = this->barcode_sdk_->GetRois();
        if (roi.width > 0)
        {
            smartmore::barcode::Roi at_roi={cv::Point2f(roi.tl()),
                                            cv::Point2f(roi.x+roi.width, roi.y),
                                            cv::Point2f(roi.br()),
                                            cv::Point2f(roi.x, roi.y+roi.height)};
            std::vector<smartmore::barcode::Roi> at_rois={at_roi};
            this->barcode_sdk_->SetRois(at_rois);
            std::cout << "barcode sdk set roi done" << std::endl;
        }
        this->barcode_sdk_->Run(input, output);
        this->barcode_sdk_->SetRois(sdk_rois); // 复原roi设置

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
                cv::Rect decode_rect = cv::boundingRect(pts);
                if(decode_rect.width > 10 && decode_rect.height > 10)
                {
                    code_regions.emplace_back(decode_rect);
                }
            }
            
            if(result.decode_succeed)
            {
                std::cout << "decoder result success " << result.result << std::endl;
                auto type_it = std::find(out_arinfo.successful_code_type.begin(), out_arinfo.successful_code_type.end(), result.type);
                if(type_it == out_arinfo.successful_code_type.end())
                {
                    out_arinfo.successful_code_type.emplace_back(result.type); // 记录解码成功的码制
                }

                if(result.type == smartmore::barcode::BarcodeType::kQrcode)
                {
                    auto ver_it = std::find(qr_versions.begin(), qr_versions.end(), result.version);
                    if(ver_it == qr_versions.end())
                    {
                        qr_versions.emplace_back(result.version);
                    }
                }
                else if(result.type == smartmore::barcode::BarcodeType::kDmcode)
                {
                    auto ver_it = std::find(dm_versions.begin(), dm_versions.end(), result.version);
                    if(ver_it == dm_versions.end())
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

const std::map<std::string, std::string> BARCODE_CONFIG_SIGNATURES = {
    {"vs1000p", "nxp100w"},
    {"vs1000p_2mp", "nxp200w"},
    {"vs2000_5mp", "nxp500w"},
    {"vs2000_20mp", "nxp2000w"},
    {"vs800p", "novaic150w"}
};

#endif //SM_BARCODE_WRAPPER_H