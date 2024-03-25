#ifndef AF_CONFIG_H
#define AF_CONFIG_H

namespace af {
    struct AFConfig{
        int max_n_samples = 10;  // the maximum number of samples
        int hmap_pos_step = 30;  // the sampling step of the heat map
        int win_size = 5;        // the size of the sliding window
    }af_alg_conf;
}

#endif  // AF_CONFIG_H