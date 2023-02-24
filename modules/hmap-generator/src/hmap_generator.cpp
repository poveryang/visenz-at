#include "hmap_generator.h"


HeatMapGenerator::HeatMapGenerator(const std::string &context_name, const std::string &precision) {
    context_name_ = context_name;
    precision_ = precision;
}

void HeatMapGenerator::Init(const std::string& model_path) {
    /* Set precision */
    int unit_size;
    int opt_precision;
    if (precision_ == "fp32"){
        opt_precision = TENGINE_MODE_FP32;
        unit_size = 4;
    } else if (precision_ == "uint8"){
        opt_precision = TENGINE_MODE_UINT8;
        unit_size = 1;
    } else {
        fprintf(stderr, "Precision not supported.\n");
        exit(1);
    }

    /* init tengine */
    if (init_tengine() != 0) {
        fprintf(stderr, "Init tengine failed.\n");
        exit(1);
    }

    /*set runtime options*/
    opt_.num_thread = 1;
    opt_.cluster = TENGINE_CLUSTER_ALL;
    opt_.precision = opt_precision;
    opt_.affinity = 0;

    /* create context */
    context_t context = nullptr;
    if (context_name_ == "timvx"){
        /* create VeriSilicon TIM-VX backend */
        context = create_context("timvx", 1);
        int rtt = set_context_device(context, "TIMVX", nullptr, 0);
        if (rtt < 0)
        {
            fprintf(stderr, " add_context_device VSI DEVICE failed.\n");
            exit(1);
        }
    }

    /* load model */
    graph_ = create_graph(context, "tengine", model_path.c_str());
    if (graph_ == nullptr) {
        fprintf(stderr, "Create graph failed.\n");
        exit(1);
    } else {
        fprintf(stdout, "Load HMap-Generator model & create graph success.\n");
    }

    /* get input tensor quantization params */
    input_tensor_ = get_graph_input_tensor(graph_, 0, 0);
    if (input_tensor_ == nullptr) {
        fprintf(stderr, "Get input tensor failed\n");
        exit(1);
    }
    get_tensor_quant_param(input_tensor_, &input_scale, &input_zero_point, 1);

    /* set input tensor shape */
    int img_h = 400;
    int img_w = 640;
    int img_c = 1;
    input_buffer_size_ = img_h * img_w * img_c * unit_size;
    int in_dims[4] = {1, img_c, img_h, img_w}; // nchw
    if (set_tensor_shape(input_tensor_, in_dims, 4) < 0) {
        fprintf(stderr, "Set input tensor shape failed\n");
        exit(1);
    }

    /* prerun graph, set work options(num_thread, cluster, precision) */
    if (prerun_graph_multithread(graph_, opt_) < 0) {
        fprintf(stderr, "Prerun multithread graph failed.\n");
        exit(1);
    }

    /* get output tensor quantization params */
    output_tensor_ = get_graph_output_tensor(graph_, 0, 0);
    if (get_tensor_shape(output_tensor_, out_dim_, 4) < 0) {
        fprintf(stderr, "Get output tensor shape failed\n");
        exit(1);
    }
    get_tensor_quant_param(output_tensor_, &output_scale, &output_zero_point, 1);
}

cv::Mat HeatMapGenerator::Infer(cv::Mat &image) {
    cv::Mat in_img = PostProcess(image);
    cv::Mat out_img;
    if (precision_ == "fp32") {
        out_img = InferFP32(in_img);
    } else if (precision_ == "uint8") {
        out_img = InferUInt8(in_img);
    } else {
        fprintf(stderr, "Precision not supported.\n");
        exit(1);
    }

    out_img = PreProcess(out_img);
    return out_img;
}

cv::Mat HeatMapGenerator::InferFP32(cv::Mat &image) {
    /* 1. set image data to input tensor */
    auto *image_data = image.data;
    if (set_tensor_buffer(input_tensor_, image_data, input_buffer_size_) < 0) {
        fprintf(stderr, "Set input tensor buffer failed\n");
        exit(1);
    }

    /* 2. run the graph */
    if (run_graph(graph_, 1) < 0) {
        fprintf(stderr, "Run graph failed.\n");
        exit(1);
    }

    /* 3. get the heatmap data from output tensor and transform to cv::Mat */
    auto output_fp32 = (float *) get_tensor_buffer(output_tensor_);
    if (output_fp32 == nullptr) {
        fprintf(stderr, "Get output data failed\n");
        exit(1);
    }
    cv::Mat heatmap = cv::Mat(out_dim_[1], out_dim_[2], CV_32FC3, output_fp32);

    return heatmap;
}

cv::Mat HeatMapGenerator::InferUInt8(cv::Mat &image) {
    /* 1. set image data to input tensor */
    auto *image_data = image.data;
    if (set_tensor_buffer(input_tensor_, image_data, input_buffer_size_) < 0) {
        fprintf(stderr, "Set input tensor buffer failed\n");
        exit(1);
    }

    /* 2. run the graph */
    if (run_graph(graph_, 1) < 0) {
        fprintf(stderr, "Run graph failed.\n");
        exit(1);
    }

    /* 3. get the heatmap data from output tensor and transform to cv::Mat */
    auto output_uint8 = (uint8_t *) get_tensor_buffer(output_tensor_);
    if (output_uint8 == nullptr) {
        fprintf(stderr, "Get output data failed\n");
        exit(1);
    }
    cv::Mat heatmap = cv::Mat(out_dim_[1], out_dim_[2], CV_8UC3, output_uint8);

    return heatmap;
}

HeatMapGenerator::~HeatMapGenerator() {
    /* release tengine */
    postrun_graph(graph_);
    destroy_graph(graph_);
    release_tengine();
}

cv::Mat HeatMapGenerator::PreProcess(cv::Mat &image) const {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(640, 400));
    if (precision_ == "fp32"){
        dst_img.convertTo(dst_img, CV_32FC3, 1.0 / 255);
        dst_img = (dst_img - cv::Scalar(0.4329))/ cv::Scalar(0.2349);
    } else if (precision_ == "uint8"){
        dst_img = dst_img * input_scale + input_zero_point;
    }
    return dst_img;
}

cv::Mat HeatMapGenerator::PostProcess(cv::Mat &image) const {
    cv::Mat dst_img;
    cv::resize(image, dst_img, cv::Size(1280, 800), 0, 0, cv::INTER_NEAREST);
    if (precision_ == "fp32"){
        dst_img.convertTo(dst_img, CV_8UC3, 255);
    }
    return dst_img;
}
