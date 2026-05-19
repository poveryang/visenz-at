/*
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:48:02
 * @Copyright: Copyright (c) 2022
 */
#ifndef AF_INTERFACE_H
#define AF_INTERFACE_H

#include "af_impl.h"

namespace af {
    class AFInterface {
    public:
        bool end_iter;                      // flag to indicate whether the AF algorithm is done
        int next_pos;                       // the next position of the lens
        int best_pos;                       // the best position of the lens

        bool start_fit = false;

    public:
        /** @brief The default constructor. */
        AFInterface();

        /** @brief The default destructor. */
        ~AFInterface() = default;

        /** @brief Init the AF algorithm
         *  @param start_pos the start position of the lens
         *  @param end_pos the end position of the lens
         *  @param image_size the size of the input image
         *  @param enable_hmap flag to indicate whether the heat map is used
         */
        void Init(AFConf &af_conf);

        /** @brief Run the AF algorithm
         *  @param img the input image
         *  @param hmap the heat map of the input image
         */
        void Run(const cv::Mat &img, const std::vector<cv::Rect> &rois={});

        void ResetSamples();

    private:
        /* the implementation object of the AF algorithm */
        std::shared_ptr<AFImpl> af_impl_;
    };
}

#endif //AF_INTERFACE_H
