#include <iostream>
#include <unistd.h>
#include <opencv2/opencv.hpp>

#ifdef USE_RKNN
#include "hmap_infer_rknn.h"
#endif

int main(int argc, char **argv) {
    /* Parse arguments */
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " -m model_path -i image_path" << std::endl;
        return 0;
    }

    std::string model_path;
    std::string image_path;

    int ch;
    while ((ch = getopt(argc, argv, "h:m:i:")) != -1) {
        switch (ch) {
            case 'h':
                std::cout << "Usage: " << argv[0]
                          << " -m model_path -i image_path" << std::endl;
                std::cout << "Options:\n"
                          << "  -m model_path Set the model path\n"
                          << "  -i image_path Set the image path (default: ./test.png)\n";
                return 0;
            case 'm':
                model_path = optarg;
                break;
            case 'i':
                image_path = optarg;
                break;
            default:
                std::cout << "Usage: " << argv[0] << " -m model_path -i image_path" << std::endl;
                exit(1);
        }
    }

    /* Inference */
    cv::Mat image = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
    std::shared_ptr<HMapInferBase> hmap_infer;
#ifdef USE_RKNN
    hmap_infer = std::make_shared<HMapInferRK>();
    hmap_infer->Init(model_path);
    cv::Mat heatmap = hmap_infer->Inference(image);
#endif
    cv::imwrite("./heatmap.png", heatmap);

    /* Blend image and heatmap */
    cv::Mat blend_img;
    cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
    cv::cvtColor(heatmap, heatmap, cv::COLOR_RGB2BGR);
    cv::resize(heatmap, heatmap, image.size());
    cv::addWeighted(image, 0.5, heatmap, 0.5, 0, blend_img);
    cv::imwrite("./blend.png", blend_img);

    return 0;
}

