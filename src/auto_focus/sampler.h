#ifndef VISENZ_AF_SPARSE_SAMPLING_H
#define VISENZ_AF_SPARSE_SAMPLING_H


#include <vector>
#include <iostream>
#include <limits>
#include <valarray>


class Sampler {
public:
    bool reverse;
    double range_length;
    int max_n_samples;
    int border_left_;
    int border_right_;

public:
    Sampler() = default;

    ~Sampler() = default;

    void Init(int border_left, int border_right, int max_n);

    std::vector<int> StepSampling(int start, int end, int step);

    std::vector<int> SparseSampling(int start, int end, int center);

private:
    static double NormInvCDF(double p, double mu, double sigma);
};


#endif //VISENZ_AF_SPARSE_SAMPLING_H
