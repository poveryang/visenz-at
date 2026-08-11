#include "at_interface.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Expect(bool cond, const std::string &msg)
{
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

CamConf MakeConf()
{
    CamConf conf;
    conf.min_intensity = 0;
    conf.max_intensity = 1;
    conf.min_et = 1;
    conf.max_et = 40000;
    conf.min_eg = 1;
    conf.max_eg = 128;
    conf.ae_mode = 1;
    conf.init_et = 1000;
    conf.init_eg = 10;
    conf.init_intensities = {1, 1, 1, 1};
    conf.start_pos = 0;
    conf.end_pos = 420;
    conf.init_pos = 0;
    conf.coarse_af_step = 20;
    conf.refine_af_step = 4;
    conf.min_et_step = 500;
    return conf;
}

cv::Mat MakeGrayFrame(int brightness)
{
    cv::Mat image(120, 160, CV_8UC1);
    image.setTo(cv::Scalar(brightness));
    return image;
}

}  // namespace

int main()
{
    CamConf conf = MakeConf();
    at::ATInterface at(false);
    at.Init(conf, false, true, true);

    const CamParams first = at.GetNextParams();
    Expect(first.exp_time == 1000, "init exposure should be 1000");
    Expect(first.exp_gain == 10, "init gain should be 10");
    Expect(first.focus_pos == 0, "init focus should be 0");
    Expect(first.lights.size() == 4, "init lights size");

    bool finished = false;
    int steps = 0;
    CamParams current = first;
    while (!finished && steps < 80) {
        // Product order: GetNextParams already done for first; then set/capture/Run.
        finished = at.Run(MakeGrayFrame(40 + (steps % 20)));
        current = at.GetNextParams();
        ++steps;
        if (!finished) {
            // Keep requesting next params like product loop.
            current = at.GetNextParams();
        }
    }

    Expect(finished, "compat AT should finish within budget");
    Expect(steps > 1, "compat AT should take more than one step");
    const CamParams best = at.GetBestParams();
    Expect(best.exp_time >= 1, "best exposure valid");
    Expect(!at.GetVersion().empty(), "version non-empty");
    (void)current;

    if (failures == 0) {
        std::cout << "at_compat_unit_test PASS steps=" << steps
                  << " version=" << at.GetVersion() << "\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "at_compat_unit_test FAIL count=" << failures << "\n";
    return EXIT_FAILURE;
}
