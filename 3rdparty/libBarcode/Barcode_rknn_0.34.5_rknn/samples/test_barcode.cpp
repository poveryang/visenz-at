#include "barcode.h"
#include "common.h"
#include <opencv2/imgproc.hpp>
#include <string>
#include <sstream>
#include <locale>
#include <regex>
#ifndef DEFINE_PROFILE_AS_EMTPY
#include "tools/profile.h"
#else // DEFINE_PROFILE_AS_EMTPY
#define PROFILE
#endif // DEFINE_PROFILE_AS_EMTPY
#include <opencv2/opencv.hpp>
#include <regex>
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <bits/getopt_core.h>
#endif

// #define SAVE_QUALITY_RESULT
// #define SAVE_FAILURE_IMAGES
// #define DEBUG_DIR "./cmake-build-debug/output/"
// #define DEBUG_DIR "./tmp_imgs/"

#ifdef DEBUG_DIR
#include "common/imgproc.h"
#endif

int main(int argc, char **argv)
{
    std::string directory_path;
    smartmore::barcode::BarcodeType barcode_type =
        smartmore::barcode::BarcodeType::kUnknown;
    unsigned int barcode_num = 1;
    smartmore::barcode::RunningMode mode =
        smartmore::barcode::RunningMode::kPerformance;
    std::string param_cfg_path = "";
    std::string default_cfg_dir = "./config/default/";
    std::string output_json_path = "";
    bool immediate_write_json = false;
    std::string signature = "";
    int loop_counts = 1;
    bool roi_check = false;
    int image_limitation = INT_MAX;
    bool is_mem_monitor = false;

    std::string iso_standard_str = "";
    smartmore::barcode::ISOStandard iso_standard =
        smartmore::barcode::ISOStandard::kISO1541x;

    std::string help_info = R"""(
        -d directory path
        -t barcode type, can be [1D, 2D, CODE_39, CODE_93, CODE_128, EAN_8, EAN_13, ITF, UPC_A, UPC_E, UPC_EAN_EXTENSION, QR, DM, PHARMA_CODE, Pdf417, Vericode, MQR], multi-type input is supported
        -n (optional) barcode nums input, default 1
        -m (optional) enable efficiency mode, default 0
        -c (optional) param config json file
        -e (optional) default config dir, default ./config/default/
        -o (optional) output json path, default no output
        -M (optional) if you have lots of pic. [> 1w]; write output json to file while decoding for save memory, default write file in the end
        -l (optional) loop, can be an integer number:
                      negative number means infinite loops scan directory, while others means loop scan times.
                      default 1
        -r (optional) roi flag for ci check, roi is image size. default false.
        -f (optional) filter for big images, the limitation is [input number] x 10000. For example, if input is 510, the image_limitation is 5, 100, 000. default is INT_MAX.
        -s (optional) signature, specify configure type, such as nxp100w,nxp200w,trt,onnxruntime and so on.
        -i (optional) iso standard, quality verify iso stand: iso1541x, iso29158
        -h help
    )""";

    int ret;
#if defined(_WIN32) || defined(_WIN64)
    while (-1 != (ret = WinGetOpt(argc, argv, "d:t:n:m:c:e:Mo:l:rf:s:i:h")))
#else
    while (-1 != (ret = getopt(argc, argv, "d:t:n:m:c:e:Mo:l:rf:s:i:wh")))
#endif
    {
        switch (ret)
        {
        case 'd':
            directory_path = optarg;
            break;
        case 't':
            barcode_type = barcode_type | StringToType(optarg);
            break;
        case 'n':
            barcode_num = atoi(optarg);
            break;
        case 'm':
            mode = (atoi(optarg) == 1)
                       ? smartmore::barcode::RunningMode::kEfficiency
                       : smartmore::barcode::RunningMode::kPerformance;
            break;
        case 'c':
            param_cfg_path = optarg;
            break;
        case 'e':
            default_cfg_dir = optarg;
            break;
        case 'o':
            output_json_path = optarg;
            break;
        case 'M':
            immediate_write_json = true;
            break;
        case 'l':
            loop_counts = atoi(optarg);
            break;
        case 'r':
            roi_check = true;
            break;
        case 'f':
            image_limitation = atoi(optarg) * 10000;
            break;
        case 's':
            signature = optarg;
            break;
        case 'i':
            iso_standard_str = optarg;
            iso_standard = StringToISOStand(optarg);
            break;
        case 'h':
            std::cout << help_info << std::endl;
            return 0;
        case 'w':
            is_mem_monitor = true;
            break;
        default:
            std::cout << "unkown cmd!" << std::endl;
            std::abort();
        }
    }
#if TARGET_OS_IPHONE
    directory_path = GeneratePathForIOS(directory_path);
    if (param_cfg_path != "")
    {
        param_cfg_path = GeneratePathForIOS(param_cfg_path);
    }
    default_cfg_dir = GeneratePathForIOS(default_cfg_dir);
    output_json_path = GeneratePathForIOS(output_json_path);
#endif

    std::cout << argv[0] << " -d " << directory_path << " -t " << barcode_type
              << " -c " << param_cfg_path << " -e " << default_cfg_dir << " -o "
              << output_json_path << " -l " << loop_counts << "-m " << mode
              << std::endl;

    if ("" == signature && param_cfg_path != "")
    {
        signature = GetSignatureFromFilename(param_cfg_path);
        param_cfg_path = "./config/default/config.json";
    }
    std::cout << "signature: " << signature << std::endl;
    std::vector<std::string> file_list;
    DfsFolder(directory_path, file_list);
    if (file_list.size() == 0)
    {
        return 0;
    }

    param_cfg_path = param_cfg_path.empty() ? (default_cfg_dir + "config.json")
                                            : param_cfg_path;

    // you can also use default ctor
    // smartmore::barcode::Barcode sdk;
    smartmore::barcode::Barcode sdk(default_cfg_dir);

#ifdef BUILD_WITH_VERIDECODE
    if ((barcode_type & smartmore::barcode::BarcodeType::kVericode) !=
        smartmore::barcode::BarcodeType::kUnknown)
    {
        sdk.SetVericodePassword("DEMO");
        sdk.SetVericodeEnable(true);
    }
#endif

    // call SetConfigSignature  before LoadConfig
    if (signature != sdk.GetConfigSignature())
    {
        sdk.SetConfigSignature(signature);
        sdk.LoadConfig(param_cfg_path);
    }

    if (iso_standard_str != "")
    {
        sdk.SetISOStandard(iso_standard);
        sdk.Set2DQualityEnable(true);
        if (smartmore::barcode::ISOStandard::kISO1541x == iso_standard)
        {
            sdk.Set1DQualityEnable(true);
        }
    }

#ifdef WITH_NOVAIC
    // cv::Point2f handheld_center = {0.f, 0.f};
    // cv::Point2f handheld_center = {340.f, 545.f};
    cv::Point2f handheld_center = {544.f, 644.f};
    sdk.SetHandheldMode(
        smartmore::barcode::HandheldModeType::kUnSet, // kCenterROI
        handheld_center);
    sdk.SetHandheldCenterWH({100, 100});

    // mem init
    NovaicInitComMem();
#endif

    sdk.SetLogEnable(false);
    sdk.SetBarcodeType(barcode_type);
    sdk.SetDMPyramidLevel(6);
    sdk.SetRunningMode2D(mode);
    sdk.SetRunningMode1D(mode);
    // sdk.SetDmECCType(smartmore::barcode::DMECCType::kAuto);

    // no barcode num input use default
    if (barcode_num >= 1)
    {
        sdk.SetNumsMaxOutput1D(barcode_num);
        sdk.SetNumsMaxOutput2D(barcode_num);
    }
    std::cout << "Barcode Version : " << sdk.GetVersion() << std::endl;
    // sdk.SetTimeoutPeriod(1000);

#if defined(SAVE_QUALITY_RESULT)
    std::ofstream quality_csv_file(directory_path + "/quality.csv");
#endif // SAVE_QUALITY_RESULT

#if defined(_PROFILE)
    std::ofstream csv_file(directory_path + "/output.csv");
#endif //_PROFILE

    nlohmann::ordered_json json_file;
    std::ofstream output_json_file_stream(output_json_path);
    if (!output_json_path.empty())
    {
        json_file["date_time"] = GetDateTime();
        json_file["version"] = sdk.GetVersion();
        json_file["dataset_path"] = directory_path;
        if (!immediate_write_json)
        {
            json_file["decode_results"] = nlohmann::ordered_json::array();
        }
    }

    std::cout << "Folder " << directory_path << " : " << file_list.size()
              << " images." << std::endl;
    double success_avg_duration = 0, failure_avg_duration = 0, avg_duration = 0;
    double max_duration = 0, min_duration = 10000000;

    int success_count = 0, failure_count = 0, count = 0;
    int decode_qr = 0, decode_dm = 0, decode_pdf417 = 0, decode_oned = 0,
        decode_vericode = 0, decode_mqr = 0, total_num = 0;

    // for memory monitor
#if defined(__unix__)
    std::vector<ProcessMem> mem_monitor;
#endif
    auto start_point = std::chrono::high_resolution_clock::now();

    while (loop_counts != 0)
    {
        if (loop_counts > 0)
        {
            loop_counts--;
        }
        for (int i = 0; i < file_list.size(); i++)
        {
            std::cout << "-------------------------------------------"
                      << std::endl;
            std::string input_image_path = file_list[i];
            std::cout << "Image[" << i << "] : " << input_image_path
                      << std::endl;

            cv::Mat img = cv::imread(input_image_path, 0);
            if (img.data == NULL)
            {
                continue;
            }

#ifdef _IMAGE_RESIZE
#if defined(__ANDROID__) || TARGET_OS_IPHONE
            TransformResize(img, 1920, 1080);
#else
            int input_img_w = 1280;
            int input_img_h = 800;
            if (param_cfg_path != "")
            {
                std::ifstream ifs(param_cfg_path);
                auto signature = sdk.GetConfigSignature();
                auto params_cfg = nlohmann::json::parse(ifs)[signature];

                if (params_cfg.contains("dl_detector_params"))
                {
                    const auto &det_params = params_cfg["dl_detector_params"];

                    bool dl_detector_enable = true;
                    if (det_params.contains("dl_detector_enable"))
                    {
                        det_params["dl_detector_enable"].get_to(
                            dl_detector_enable);
                    }

                    if (dl_detector_enable)
                    {
                        input_img_w = det_params.contains("input_img_w")
                                          ? det_params["input_img_w"].get<int>()
                                          : input_img_w;
                        input_img_h = det_params.contains("input_img_h")
                                          ? det_params["input_img_h"].get<int>()
                                          : input_img_h;
                    }
                }

                std::cout << "config input size as [" << input_img_w << ","
                          << input_img_h << "]" << std::endl;
            }

            TransformResize(img, input_img_h, input_img_w);
#endif // __ANDROID__ || TARGET_OS_IPHONE
#endif // _IMAGE_RESIZE

            const int image_size = img.rows * img.cols;
            if (image_size > image_limitation)
            {
                std::cout << "Image size: " << image_size << std::endl;
                std::cout << "Image limitation: " << image_limitation
                          << std::endl;
                std::cout << "Image size is out of limitation, skip."
                          << std::endl;
                continue;
            }
            smartmore::barcode::BarcodeRequest input(img.data, img.cols,
                                                     img.rows);
            // smartmore::barcode::BarcodeRequest input = {img};
            smartmore::barcode::BarcodeResponse output;
            bool status;
            auto start_time = std::chrono::high_resolution_clock::now();
            try
            {
                PROFILE(status = sdk.Run(input, output));
            }
            catch (std::exception &e)
            {
                std::cout << "sdk run error : " << e.what() << std::endl;
                abort();
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            double duration_ms =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time)
                    .count() /
                1000.f;
            avg_duration += duration_ms;

            max_duration = std::max(max_duration, duration_ms);
            min_duration = std::min(min_duration, duration_ms);

            count++;
            std::cout << "duration : " << duration_ms << " ms" << std::endl;
            if (status)
            {
                std::cout << "<<<success" << std::endl;
                success_avg_duration += duration_ms;
                success_count++;
            }
            else
            {
                std::cout << "<<<failure" << std::endl;
                failure_avg_duration += duration_ms;
                failure_count++;
            }
#ifdef DEBUG_DIR
            smartmore::barcode::ImageFilter image_filter;
            img = image_filter.Run(img, sdk.GetImageFilterOps(), 1.);
            cv::Mat draw;
            cv::cvtColor(img, draw, cv::COLOR_GRAY2BGR);
            cv::putText(draw,
                        std::to_string(static_cast<int>(duration_ms)) + " ms",
                        cv::Point(10, img.rows - 10), cv::FONT_HERSHEY_PLAIN,
                        2.0, cv::Scalar(0, 0, 255));
            int idx = 0;
            int decode_succeed_single = 0;
            for (auto &result : output.results)
            {
                std::vector<cv::Point> pts = {
                    result.coarse_locate_corner_boxes[0],
                    result.coarse_locate_corner_boxes[1],
                    result.coarse_locate_corner_boxes[2],
                    result.coarse_locate_corner_boxes[3]};
                cv::polylines(draw, pts, true, cv::Scalar(255, 0, 0));
                cv::putText(draw, "idx:" + std::to_string(idx),
                            result.precise_locate_corner_boxes[idx % 4],
                            cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(0, 255, 0));
                idx += 1;
                if (result.succeed)
                {
                    decode_succeed_single += 1;
                    cv::putText(
                        draw, result.result, result.corner_boxes[idx % 4],
                        cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(0, 255, 0));
                    std::vector<cv::Point> pts = {
                        result.corner_boxes[0], result.corner_boxes[1],
                        result.corner_boxes[2], result.corner_boxes[3]};
                    cv::polylines(draw, pts, true, cv::Scalar(0, 255, 0));
                    cv::circle(draw, result.corner_boxes[0], 3,
                               cv::Scalar(0, 255, 0), -1);
                    cv::circle(draw, result.corner_boxes[3], 3,
                               cv::Scalar(0, 255, 255), -1);
                }
                if (result.detect_succeed)
                {
                    std::vector<cv::Point> pts = {
                        result.precise_locate_corner_boxes[0],
                        result.precise_locate_corner_boxes[1],
                        result.precise_locate_corner_boxes[2],
                        result.precise_locate_corner_boxes[3]};
                    if (result.type == smartmore::barcode::BarcodeType::kQrcode)
                    {
                        cv::polylines(draw, pts, true, cv::Scalar(0, 255, 255));
                    }
                    else if (result.type ==
                                 smartmore::barcode::BarcodeType::kDmcode ||
                             result.type ==
                                 smartmore::barcode::BarcodeType::kVericode)
                    {
                        cv::polylines(draw, pts, true, cv::Scalar(255, 0, 255));
                    }
                    else if ((result.type &
                              smartmore::barcode::BarcodeType::kCode1D) !=
                             smartmore::barcode::BarcodeType::kUnknown)
                    {
                        cv::polylines(draw, pts, true, cv::Scalar(255, 255, 0));
                    }
                    else if ((result.type &
                              smartmore::barcode::BarcodeType::kPdf417) !=
                             smartmore::barcode::BarcodeType::kUnknown)
                    {
                        cv::polylines(draw, pts, true, cv::Scalar(127, 255, 0));
                    }
                }
            }

            cv::putText(draw,
                        "decode_num: " + std::to_string(decode_succeed_single),
                        cv::Point(200, img.rows - 10), cv::FONT_HERSHEY_PLAIN,
                        2.0, cv::Scalar(0, 0, 255));
            std::string image_path(DEBUG_DIR);
            std::string full_name = input_image_path.substr(
                input_image_path.rfind("/") + 1,
                input_image_path.size() - input_image_path.rfind("/"));
            std::string name = full_name.substr(0, full_name.rfind("."));
            image_path = image_path + name + ".jpg";
            std::cout << "save img path: " << image_path << std::endl;
            cv::imwrite(image_path, draw);
#endif
            if (!output.results.empty())
            {
                for (const auto &result : output.results)
                {
                    if (result.succeed)
                    {
                        total_num++;
                        if (result.type ==
                            smartmore::barcode::BarcodeType::kQrcode)
                        {
                            decode_qr++;
                        }
                        else if (result.type ==
                                 smartmore::barcode::BarcodeType::kDmcode)
                        {
                            decode_dm++;
                        }
                        else if (result.type ==
                                 smartmore::barcode::BarcodeType::kPdf417)
                        {
                            decode_pdf417++;
                        }
                        else if (result.type ==
                                 smartmore::barcode::BarcodeType::kVericode)
                        {
                            decode_vericode++;
                        }
                        else if (result.type ==
                                 smartmore::barcode::BarcodeType::kMicroQR)
                        {
                            decode_mqr++;
                        }
                        else
                        {
                            decode_oned++;
                        }

#if defined(SAVE_QUALITY_RESULT)
                        WriteQualityBuffer(quality_csv_file, input_image_path,
                                           sdk.GetISOStandard(), result,
                                           (total_num == 1));
#endif
                    }
                    PrintResult(result);
                }
#ifdef _PROFILE
                WriteCsvBuffer(csv_file, input_image_path,
                               output.results[0].corner_boxes,
                               output.results[0].result);
#endif //_PROFILE
            }
            else
            {
#ifdef SAVE_FAILURE_IMAGES
                const size_t pos =
                    std::max(file_list[i].find_last_of('/') + 1,
                             file_list[i].find_last_of('\\') + 1);
                const std::string input_image_name =
                    file_list[i].substr(pos, file_list[i].size());
                std::string output_path =
                    std::string("/mnt/d/output/") + input_image_name;
                std::cout << output_path << std::endl;
                cv::imwrite(output_path, img);
#endif
#ifdef _PROFILE
                WriteCsvBuffer(csv_file, input_image_path, {}, "");
#endif //_PROFILE
            }
            if (!output_json_path.empty())
            {
                nlohmann::ordered_json output_json_buffer;
                WriteJsonFileBuffer(output_json_buffer, input_image_path,
                                    output, duration_ms, status);
                if (immediate_write_json)
                {
                    output_json_file_stream << output_json_buffer.dump()
                                            << "\n";
                }
                else
                {
                    json_file["decode_results"].push_back(output_json_buffer);
                }
            }
            if (roi_check)
            {
                std::vector<smartmore::barcode::Roi> rois{
                    {cv::Point2f(0.f, 0.f), cv::Point2f(img.cols - 0.001f, 0.f),
                     cv::Point2f(0.f, img.rows - 0.001f),
                     cv::Point2f(img.cols - 0.001f, img.rows - 0.001f)}};
                sdk.SetRois(rois);
                smartmore::barcode::BarcodeRequest roi_input = {img};
                smartmore::barcode::BarcodeResponse roi_output;
                bool roi_status;
                roi_status = sdk.Run(roi_input, roi_output);
                if (roi_status ^ status)
                {
                    std::cerr << "The exception of different result using roi!"
                              << std::endl;
                    throw;
                }
                if (roi_status && !(CompareBarcodeResponse(output, roi_output,
                                                           img.rows, img.cols)))
                {
                    throw;
                }
                sdk.ClearRois();
            }

#if defined(__unix__)
            if (is_mem_monitor)
            {
                // memory leak monitor
                auto mem_process = GetProcessMemory();
                std::cout << "virtual_mem: " << mem_process.virtual_mem
                          << std::endl;
                std::cout << "physical_mem: " << mem_process.physical_mem
                          << std::endl;
                std::cout << "virtual_mem_max: " << mem_process.virtual_mem_max
                          << std::endl;
                std::cout << "physical_mem_max: "
                          << mem_process.physical_mem_max << std::endl;
                auto diff_time =
                    std::chrono::duration_cast<std::chrono::minutes>(
                        std::chrono::high_resolution_clock::now() - start_point)
                        .count();
                std::cout << "diff_time: " << diff_time << " "
                          << "\nmem_process.virtual_mem: "
                          << mem_process.virtual_mem << " "
                          << "\nmem_process.virtual_mem_max: "
                          << mem_process.virtual_mem_max << " "
                          << "\nmem_process.physical_mem: "
                          << " " << mem_process.physical_mem << " "
                          << "\nmem_process.physical_mem_max: "
                          << mem_process.physical_mem_max << std::endl;

                if (diff_time > 200)
                {
                    // if program takes than 3 hour, finish memory monitor
                    std::cout << "Memory monitor finished. No error."
                              << std::endl;
                    return 0;
                }
                // if program takes than 1 hour, start to record and compare
                // memory
                else if (diff_time >= 120)
                {
                    if (mem_monitor.empty())
                    {
                        mem_monitor.emplace_back(mem_process);
                    }
                    // if diff is more than 30 MB, throw an exception
                    else if ((mem_process.physical_mem -
                              mem_monitor[0].physical_mem) /
                                 1024.f >
                             10.f)
                    {
                        std::cout << "\nmem_monitor[0].virtual_mem: "
                                  << mem_monitor[0].virtual_mem << " "
                                  << "\nmem_monitor[0].virtual_mem_max: "
                                  << mem_monitor[0].virtual_mem_max << " "
                                  << "\nmem_monitor[0].physical_mem: "
                                  << " " << mem_monitor[0].physical_mem << " "
                                  << "\nmem_monitor[0].physical_mem_max: "
                                  << mem_monitor[0].physical_mem_max
                                  << std::endl;
                        throw std::runtime_error("Memory leaks than 10 MB!");
                    }
                }
                if (0 == loop_counts)
                {
                    ++loop_counts;
                }
            }
#endif
        }
    }

#if defined(SAVE_QUALITY_RESULT)
    quality_csv_file.close();
#endif //_PROFILE

#if defined(_PROFILE)
    std::cout << "write profile file" << std::endl;
    smartmore::Profiler::GetInstance().PrintProfile(directory_path +
                                                    "/profile.csv");
    csv_file.close();
#endif //_PROFILE

    std::cout << "-------------------------------------------" << std::endl;
    if (!output_json_path.empty())
    {
        json_file["dm_decode"] = decode_dm;
        json_file["qr_decode"] = decode_qr;
        json_file["pdf417_decode"] = decode_pdf417;
        json_file["mqr_decode"] = decode_mqr;
        json_file["oned_decode"] = decode_oned;
        json_file["count"] = count;
        json_file["success_count"] = success_count;
        json_file["fail_count"] = failure_count;
        json_file["average_duration"] = avg_duration / count;
        json_file["success_average_duration"] =
            success_avg_duration / success_count;
        json_file["failure_average_duration"] =
            failure_avg_duration / failure_count;
        if (!output_json_file_stream.is_open())
        {
            std::cout << "save " << output_json_path << " failed" << std::endl;
        }
        else
        {
            try
            {
                output_json_file_stream << std::setw(4) << json_file;
                std::cout << "save " << output_json_path << " success"
                          << std::endl;
            }
            catch (std::exception)
            {
                std::cout << "save " << output_json_path << " failed"
                          << std::endl;
            }
        }
        output_json_file_stream.close();
    }

    std::cout << "image count : " << count << ", ";
    std::cout << "image success count : " << success_count << ", ";
    std::cout << "image failure count : " << failure_count << std::endl;
    std::cout << "barcode decode count : " << total_num << ", ";
    std::cout << "DM : " << decode_dm << ", ";
    std::cout << "QR : " << decode_qr << ", ";
    std::cout << "Vericode : " << decode_vericode << ", ";
    std::cout << "PDF417 : " << decode_pdf417 << ", ";
    std::cout << "MicroQR : " << decode_mqr << ", ";
    std::cout << "1D : " << decode_oned << std::endl;
    std::cout << "average duration : " << avg_duration / count << " ms, ";
    std::cout << "success average duration : "
              << success_avg_duration / success_count << " ms, ";
    std::cout << "failure average duration : "
              << failure_avg_duration / failure_count << " ms" << std::endl;
    std::cout << "min duration : " << min_duration
              << " ms, max duration : " << max_duration << " ms";
    std::cout << std::endl;

#ifdef WITH_NOVAIC
    sdk.CloseNetWork();
    NovaicCommomUninit();
#endif

    return 0;
}
