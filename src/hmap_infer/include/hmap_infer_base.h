#ifndef HMAP_INFER_BASE_H
#define HMAP_INFER_BASE_H

#include <opencv2/opencv.hpp>

struct Hotspot {
    int id=-1;                                      // The id of the hotspot
    int type=-1;                                    // The type of the barcode in the hotspot
    int area=0;                                     // The area of the hotspot(note: the area isn't same as the rect)
    double sum_intensity=0.;                        // The sum intensity of the hotspot
    double mean_intensity=0.;                       // The mean intensity of the hotspot
    double max_intensity=0.;                        // The max intensity of the hotspot
    cv::Rect2d rect={0,0,0,0};    // The rectangle of the hotspot
    cv::Mat mask=cv::Mat();                         // The mask of the hotspot inside the rect
    cv::Point2d centroid={0,0};               // The centroid of the hotspot
};

class HMapInferBase {
public:
    std::vector<Hotspot> hots_recoder;

public:
    HMapInferBase() = default;

    ~HMapInferBase() = default;

    virtual void Init(const std::string &model_path) = 0;

    virtual cv::Mat Inference(const cv::Mat &image) = 0;

    virtual cv::Mat PreProcess(const cv::Mat &image) = 0;

    virtual cv::Mat PostProcess(cv::Mat &image) = 0;

    std::vector<Hotspot> LocateHotspots(const cv::Mat &heatmap, double intensity_thres);

    Hotspot LocateMaxHotspot(const cv::Mat &heatmap, double intensity_thres);

    void IdentifyHotspot(Hotspot &new_hotspot);

    static double CalcHotsIOU(const Hotspot &hot1, const Hotspot &hot2);

    inline void SetInferSize(const cv::Size2i size)
    {
        this->infer_size_wh_ = size;
    }

    inline cv::Size2i GetInferInputSize() const
    {
        return this->infer_size_wh_;
    }

    inline void SetHmapIntensityThreshold(float thre)
    {
        this->hmap_intensity_thre_ = thre;
    }

    inline float GetHmapIntensityThreshold() const
    {
        return this->hmap_intensity_thre_;
    }

protected:
    cv::Size2i infer_size_wh_ = cv::Size2i(1280, 800);
    float hmap_intensity_thre_ = 0.2;
};

#endif //HMAP_INFER_BASE_H
