/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
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
        int start_pos;                   // The start position of the lens
        int end_pos;                     // The end position of the lens
        // int init_pos;                    // The initial position of the lens
        int coarse_step;
        int refine_step;
    };

    class AFImpl {
    public:
        /* basic variables */
        bool end_iter;                          // Flag to indicate whether the AF algorithm is done
        bool start_fit;                         // Flag to indicate whether the fitting is started
        int next_pos;                           // The next position of the lens
        int best_pos;                           // The best position of the lens
        std::map<int, double> pos_with_fv;      // The positions with the corresponding focus value

        /* sampling related variables */
        Sampler sampler{};                      // The sampler object
        std::vector<int> pos_samples;           // The positions of the samples
        int border_left;                        // The left boundary of the lens position
        int border_right;                       // The right boundary of the lens position
        int pos_start;                          // Start position of the search range
        int pos_end;                            // End position of the search range
        int pos_cur_center;                     // Current center position
        int pos_prev_center;                    // Previous center position

        int coarse_step;
        int refine_step;
        int refine_range;

        /* misc */
        int win_size;                       // The size of the sliding window

    public:

        explicit AFImpl(AFConf &af_conf);

        void Run(const cv::Mat &image, const std::vector<cv::Rect> &rois={});

        void ResetSamples();

        void GetNextSample();

    private:
        void CalcFocusValue(const cv::Mat &image, const std::vector<cv::Rect> &rois={});

        void AnalysisFvCurve();

        void RefineFocus();

        void UpdateStatus(std::vector<int> &pos_vec, int peak_idx);

        int SlidingWinSearch(std::vector<double> &val_vec, int win_size, bool mono_check);

        void ApplyMaxFv(std::vector<int> &pos_vec, int &peak_idx);

        void ElasticRangeAdjust(std::vector<int> &pos_vec, int peak_idx);

        void GetPosFv(std::vector<int> &pos_vec, std::vector<double> &fv_vec);

        int fit_pos_num_ = 0;

        bool first_stage_;
    };
}


#endif //AF_IMPL_H
