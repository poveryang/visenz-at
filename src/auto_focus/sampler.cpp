#include "sampler.h"

void Sampler::Init(int border_left, int border_right, int max_n) {
    this->reverse = false;
    this->range_length = border_right - border_left;
    this->max_n_samples = max_n;
    this->border_left_ = border_left;
    this->border_right_ = border_right;
}

std::vector<int> Sampler::StepSampling(int start, int end, int step) {
    /* Calculate the sampling positions by using the step */
    std::vector<int> samples;
    for (int i = start; i <= end; i += step) {
        if (i < border_left_ || i > border_right_) {
            continue;
        }
        samples.emplace_back(i);
    }

    return samples;
}

std::vector<int> Sampler::SparseSampling(int start, int end, int center) {
    /* Calculate the sampling positions by using the iCDF */
    std::vector<int> samples;

    /* Calculate the fraction of the sampling range and the number of samples */
    double max_range = std::max(abs(center - start), abs(center - end));
    double frac_range =  2 * max_range / range_length;  // The fraction of the sampling range
    int n_samples = static_cast<int>(round(max_n_samples * pow(frac_range, 0.3)));  // The number of samples

    /* Calculate the parameters of the normal distribution */
    double sigma = abs(start - end) / 2.0;  // The standard deviation of the normal distribution
    double mu = center;
    if (frac_range > 0.5) {
        sigma *= 2.0;
    } else if (frac_range > 0.15) {
        sigma *= 1.5;
    } else {
        sigma *= 1.0;
    }

    /* The cumulative probability of the start, end positions, and the range */
    double cum_prob_start = 0.5 * (1 + erf((start - mu) / (sigma * sqrt(2.0))));
    double cum_prob_end = 0.5 * (1 + erf((end - mu) / (sigma * sqrt(2.0))));
    double prob_range = cum_prob_end - cum_prob_start;

    /* Calculate the sampling positions by using the iCDF */
    for (int i = 1; i <= n_samples; i++) {
        double cum_prob = i / (n_samples+1.0) * prob_range + cum_prob_start;
        double quantile = NormInvCDF(cum_prob, mu, sigma);
        double pos = static_cast<int>(round(quantile));
        if (pos >= start && pos <= end) {
            samples.emplace_back(static_cast<int>(round(pos)));
        }
    }

    #ifdef BUILD_WITH_LOG
        printf("SparseSampling, samples: ");
        for (int sample : samples) {
            printf("%d ", sample);
        }
        printf("\n");
    #endif

    return samples;
}

double Sampler::NormInvCDF(double p, double mu, double sigma) {
    /**
     * Inverse of the standard normal cumulative distribution function(iCDF)
     * REFERENCE:
     *          Beasley, J. D. and S. G. Springer (1977):Algorithm AS 111
     *          Wichura, M.J. (1988): Algorithm AS 241
     *
     * @param p: probability
     * @param mu: mean of the normal distribution
     * @param sigma: standard deviation of the normal distribution
     * @return: the value of the normal distribution
     */

    if (p < 0 || p > 1) {
        std::cerr << "NormInv: p must be in [0, 1]" << std::endl;
        return 0;
    } else if (p == 0) {
        return -std::numeric_limits<double>::infinity();
    } else if (p == 1) {
        return std::numeric_limits<double>::infinity();
    }

    if (sigma < 0) {
        std::cerr << "NormInv: sigma must be positive" << std::endl;
        return 0;
    } else if (sigma == 0) {
        return mu;
    }

    double r, val, q;
    q = p - 0.5;

    if (std::abs(q) <= 0.425) {  // 0.075 <= p <= 0.925
        r = 0.180625 - q * q;
        val = q * (((((((2509.0809287301226727
                         * r + 33430.575583588128105)
                        * r + 67265.770927008700853)
                       * r + 45921.953931549871457)
                      * r + 13731.693765509461125)
                     * r + 1971.5909503065514427)
                    * r + 133.14166789178437745)
                   * r + 3.387132872796366608)
              / (((((((5226.495278852854561
                       * r + 28729.085735721942674)
                      * r + 39307.89580009271061)
                     * r + 21213.794301586595867)
                    * r + 5394.1960214247511077)
                   * r + 687.1870074920579083)
                  * r + 42.313330701600911252)
                 * r + 1);
    } else {  // r = min(p, 1-p)
        if (q > 0) {
            r = 1 - p;
        } else {
            r = p;
        }

        // r = sqrt(-log(r)) <==>  min(p, 1-p) = exp(-r^2)
        r = std::sqrt(-std::log(r));

        if (r <= 5) {
            r += -1.6;
            val = (((((((7.7454501427834140764e-4
                         * r + .0227238449892691845833)
                        * r + .24178072517745061177)
                       * r + 1.27045825245236838258)
                      * r + 3.64784832476320460504)
                     * r + 5.7694972214606914055)
                    * r + 4.6303378461565452959)
                   * r + 1.42343711074968357734)
                  / (((((((1.05075007164441684324e-9
                           * r + 5.475938084995344946e-4)
                          * r + .0151986665636164571966)
                         * r + .14810397642748007459)
                        * r + .68976733498510000455)
                       * r + 1.6763848301838038494)
                      * r + 2.05319162663775882187)
                     * r + 1);
        } else { // very close to  0 or 1
            r += -5;
            val = (((((((2.01033439929228813265e-7
                         * r + 2.71155556874348757815e-5)
                        * r + .0012426609473880784386)
                       * r + .026532189526576123093)
                      * r + .29656057182850489123)
                     * r + 1.7848265399172913358)
                    * r + 5.4637849111641143699)
                   * r + 6.6579046435011037772)
                  / (((((((2.04426310338993978564e-15
                           * r + 1.4215117583164458887e-7)
                          * r + 1.8463183175100546818e-5)
                         * r + 7.868691311456132591e-4)
                        * r + .0148753612908506148525)
                       * r + .13692988092273580531)
                      * r + .59983220655588793769) * r + 1);
        }

        if (q < 0.0) {
            val = -val;
        }
    }
    return mu + sigma * val;
}
