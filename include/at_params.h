#ifndef AT_PARAMS_H
#define AT_PARAMS_H


namespace at{
    /** This is a struct that is used to store the camera parameters.*/
    struct CamParams {
        /** Light intensities*/
        std::vector<int> lights;

        /** Focus position*/
        int focus_pos;

        /** Exposure time*/
        int exp_time;

        /** Exposure gain*/
        int exp_gain;

        /** ROI of the image*/
        cv::Rect2i roi;
    };

    /** A struct that is used to store the AR parameters.*/
    struct ARParams {
        int number_1D = 0;
        int number_2D = 0;
        bool read_1D = false;
        bool read_2D = false;
        int polarity_1D = 0;
        int polarity_2D = 0;
        int mirror_2D = 0;
        int DM_2D = 2; /*0 : 正方形 1 ：长方形 2 ：兼容模式*/
        bool codelist[12] = {false};
        cv::Rect rect;

        void reset(const cv::Mat &image) {
            number_1D = 0;
            number_2D = 0;
            read_1D = false;
            read_2D = false;
            DM_2D = 2;
            polarity_1D = 0;
            polarity_2D = 0;
            mirror_2D = 0;
            std::fill(codelist, codelist + 12, false);
            rect = cv::Rect(0, 0, image.cols, image.rows);
        }

        void print()
        {
            printf("number_1D : %d\n", number_1D);
            printf("number_2D : %d\n", number_2D);
            printf("polarity_1D : %d\n", polarity_1D);
            printf("polarity_2D : %d\n", polarity_2D);
            printf("DM_2D : %d\n", DM_2D);
            printf("mirror_2D : %d\n", mirror_2D);
            for (int i = 1; i < sizeof(codelist); i++)
            {
                printf("codelist[%d] : %d\n",i , codelist[i]);
            }
        }
    };
}

#endif  // AT_PARAMS_H
