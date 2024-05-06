#ifndef AF_IMPL_H
#define AF_IMPL_H

#include <opencv2/opencv.hpp>
#include "sampler.h"

namespace af {
    struct HotspotInfo {
        int id;                           // The id of the hotspot
        int type;                         // The type of the barcode in the hotspot
        int area;                         // The area of the hotspot(note: the area isn't same as the rect)
        cv::Mat mask;                     // The mask of the hotspot inside the rect
        cv::Rect2d rect;                  // The rectangle of the hotspot
        double max_intensity;             // The max intensity of the hotspot
        std::vector<int> positions;       // The positions of the hotspot
        std::vector<double> intensities;  // The intensities of the hotspot
    };

    struct AFConf {
        /** Position control */
        int START_POS;                   // The start position of the lens
        int END_POS;                     // The end position of the lens
        int INIT_POS;                    // The initial position of the lens

        /** Resolution and ROI */
        int IMG_WIDTH;                   // The width of the image
        int IMG_HEIGHT;                  // The height of the image
        cv::Rect2d ROI;                     // roi of the image
    };

    class AFImpl {
    public:
        /* basic variables */
        bool end_iter;                          // Flag to indicate whether the AF algorithm is done
        bool start_fit;                         // Flag to indicate whether the fitting is started
        int next_pos;                           // The next position of the lens
        int best_pos;                           // The best position of the lens
        std::map<int, double> pos_with_fv;      // The positions with the corresponding focus value
        std::map<int, double> pos_fv_recorder;  // Record all positions with the corresponding focus value

        /* heat-map related parameters */
        bool enable_hmap;                       // Flag to indicate whether the heat map is used
        int hmap_pos_step;                      // The mean step size in the sampling with the heatmap
        std::vector<HotspotInfo> id_hots;       // All the identified hotspots
        HotspotInfo best_hot;

        /* roi related parameters */
        bool enable_roi;                        // Flag to indicate whether the roi is used
        cv::Rect2d roi;                         // The rectangle of the hotspot

        /* sampling related variables */
        Sampler sampler{};                      // The sampler object
        std::vector<int> pos_samples;           // The positions of the samples
        int border_left;                        // The left boundary of the lens position
        int border_right;                       // The right boundary of the lens position
        int pos_start;                          // Start position of the search range
        int pos_end;                            // End position of the search range
        int pos_cur_center;                     // Current center position
        int pos_prev_center;                    // Previous center position

        /* misc */
        int win_size;                       // The size of the sliding window
        int img_width;                   // The width of the image
        int img_height;                   // The height of the image

    public:

        explicit AFImpl(AFConf &af_conf, bool enable_hmap=false);

        void Run(const cv::Mat &image, const std::vector<cv::Rect> &rois={});

    private:
        void CalcFocusValue(const cv::Mat &image, const std::vector<cv::Rect> &rois={});

        void AnalysisHotspots();

        void AnalysisFvCurve();

        void PolynomialFit();

        void RefineFocus();

        void UpdateStatus(std::vector<int> &pos_vec, int peak_idx);

        void ResetSamples();

        void GetNextSample();

        static int SlidingWinSearch(std::vector<double> &val_vec, int win_size, bool mono_check);

        void ApplyMaxHot(std::vector<int> &pos_vec, int &peak_idx) const;

        void ApplyMaxFv(std::vector<int> &pos_vec, int &peak_idx);

        void ElasticRangeAdjust(std::vector<int> &pos_vec, int peak_idx);

        void GetPosFv(std::vector<int> &pos_vec, std::vector<double> &fv_vec);

        cv::Rect2d ExpandRect(const cv::Rect2d &rect) const;

        int fit_pos_num = 0;
    };
}


#endif //AF_IMPL_H
