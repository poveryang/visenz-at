#pragma once

#include <opencv2/core.hpp>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

/** @brief Camera configuration (legacy product contract). */
struct CamConf {
    int min_intensity = 0;
    int max_intensity = 1;

    int min_et = 1;
    int max_et = 40000;
    int min_eg = 1;
    int max_eg = 128;
    int ae_mode = 1;
    int init_et = 1000;
    int init_eg = 10;
    std::vector<int> init_intensities{1, 1, 1, 1};

    int start_pos = 0;
    int end_pos = 1023;
    int init_pos = 0;
    int coarse_af_step = 8;
    int refine_af_step = 2;
    int min_et_step = 1;
};

/** @brief Camera parameters (legacy product contract). */
struct CamParams {
    int exp_time = 1000;
    int exp_gain = 10;
    std::vector<int> lights{1, 1, 1, 1};
    int focus_pos = 0;

    void Print() const
    {
        printf("Exp time=%d, Exp gain=%d, Lights=(", exp_time, exp_gain);
        for (const auto &light : lights) {
            printf("%d,", light);
        }
        printf("); Focus pos=%d\n", focus_pos);
    }

    std::string Str() const
    {
        std::stringstream ss;
        ss << exp_time << "-" << exp_gain << "-|";
        for (const auto &light : lights) {
            ss << light << "|";
        }
        ss << "-" << focus_pos;
        return ss.str();
    }
};
