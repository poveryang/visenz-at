/*******************************************************************************
 * FILENAME:      common.h
 *
 * AUTHORS:       Shen Qi
 *
 * START DATE:    Tuesday April 6th 2021
 *
 * CONTACT:       qi.shen@smartmore.com
 *******************************************************************************/

#ifndef SMARTMORE_BARCODE_COMMON_H
#define SMARTMORE_BARCODE_COMMON_H

#include <cstdint>
#include <regex>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <nlohmann/json.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include "barcode.h"

#ifdef WITH_NOVAIC
#include "network_api.h"
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <stdio.h>
#include <string.h>
#define EPR                 fprintf(stderr,
#define ERR(str, chr)                                                          \
    if (opterr)                                                                \
    {EPR "%s%c\n", str, chr);                                                  \
    }
int opterr = 1;
int optind = 1;
int optopt;
char *optarg;

int WinGetOpt(int argc, char *const argv[], const char *opts)
{
    static int sp = 1;
    int c;
    char *cp;

    if (sp == 1)
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
            return -1;
        else if (strcmp(argv[optind], "--") == 0)
        {
            optind++;
            return -1;
        }
    optopt = c = argv[optind][sp];
    if (c == ':' || (cp = (char *)strchr(opts, c)) == 0)
    {
        ERR(": illegal option -- ", c);
        if (argv[optind][++sp] == '\0')
        {
            optind++;
            sp = 1;
        }
        return '?';
    }
    if (*++cp == ':')
    {
        if (argv[optind][sp + 1] != '\0')
            optarg = &argv[optind++][sp + 1];
        else if (++optind >= argc)
        {
            ERR(": option requires an argument -- ", c);
            sp = 1;
            return '?';
        }
        else
            optarg = argv[optind++];
        sp = 1;
    }
    else
    {
        if (argv[optind][++sp] == '\0')
        {
            sp = 1;
            optind++;
        }
        optarg = 0;
    }
    return c;
}
#endif

#if defined(__unix__)
int ParseLine(char *line)
{
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char *p = line;
    while (*p < '0' || *p > '9')
        p++;
    line[i - 3] = '\0';
    i = atoi(p);
    return i;
}

// process memory of virtual and physical，unit is KB
struct ProcessMem
{
    uint32_t virtual_mem;
    uint32_t virtual_mem_max;
    uint32_t physical_mem;
    uint32_t physical_mem_max;
};

ProcessMem GetProcessMemory()
{
    FILE *file = fopen("/proc/self/status", "r");
    char line[128];
    ProcessMem process_mem;

    while (fgets(line, 128, file) != NULL)
    {
        if (strncmp(line, "VmSize:", 7) == 0)
        {
            process_mem.virtual_mem = ParseLine(line);
        }

        if (strncmp(line, "VmPeak:", 7) == 0)
        {
            process_mem.virtual_mem_max = ParseLine(line);
        }

        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            process_mem.physical_mem = ParseLine(line);
        }

        if (strncmp(line, "VmHWM:", 6) == 0)
        {
            process_mem.physical_mem_max = ParseLine(line);
        }
    }
    fclose(file);
    return process_mem;
}
#endif

bool CreateDir(const std::string &folder_path)
{
#if defined(_WIN32) || defined(_WIN64)
    if (CreateDirectory(folder_path.c_str(), NULL) ||
        ERROR_ALREADY_EXISTS == GetLastError())
    {
        return true;
    }

    return false;
#else
    if (0 != access(folder_path.c_str(), 0))
    {
        // if this folder not exist, create a new one.
        int ret = mkdir(folder_path.c_str(), 0777); // 0 :success，-1 :fail
        std::cout << "createDir: " << folder_path << std::endl;
        return ret == 0;
    }

    return true;
#endif
}

void DfsFolder(const std::string folder_path,
               std::vector<std::string> &input_image_list)
{

#if defined(_WIN32) || defined(_WIN64)

    HANDLE dir;
    WIN32_FIND_DATA file_data;

    if (INVALID_HANDLE_VALUE ==
        (dir = FindFirstFile((folder_path + "/*").c_str(), &file_data)))
    {
        return;
    }

    do
    {
        const std::string file_name = file_data.cFileName;
        const std::string full_file_name = folder_path + "/" + file_name;
        const bool is_directory =
            (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if ('.' == file_name[0])
            continue;

        if (is_directory)
        {
            DfsFolder(full_file_name, input_image_list);
            continue;
        }

        std::string houzhui =
            file_name.substr(file_name.rfind(".") + 1, file_name.size());
        if (houzhui == "jpg" || houzhui == "JPG" || houzhui == "png" ||
            houzhui == "PNG" || houzhui == "bmp" || houzhui == "BMP" ||
            houzhui == "tiff" || houzhui == "TIFF" || houzhui == "tif" ||
            houzhui == "TIF")
        {
            input_image_list.push_back(full_file_name);
        }

    } while (FindNextFile(dir, &file_data));

    FindClose(dir);
#else
    DIR *p_dir;
    struct dirent *ptr;

    struct stat s;
    lstat(folder_path.c_str(), &s);

    if (!S_ISDIR(s.st_mode))
    {
        std::cout << "[ERROR] not a valid directory: " << folder_path
                  << std::endl;
        return;
    }

    if (!(p_dir = opendir(folder_path.c_str())))
    {
        std::cout << "[ERROR] opendir error: " << folder_path << std::endl;
        return;
    }
    std::string sub_file;
    while ((ptr = readdir(p_dir)) != 0)
    {
        sub_file = ptr->d_name;

        std::string full_path = folder_path + "/" + sub_file;
        lstat(full_path.c_str(), &s);

        if (S_ISDIR(s.st_mode))
        {
            if (sub_file == "." || sub_file == "..")
            {
                continue;
            }

            DfsFolder(full_path, input_image_list);
        }
        else
        {
            std::string houzhui =
                sub_file.substr(sub_file.rfind(".") + 1, sub_file.size());
            if (houzhui == "jpg" || houzhui == "JPG" || houzhui == "png" ||
                houzhui == "PNG" || houzhui == "bmp" || houzhui == "BMP" ||
                houzhui == "tiff" || houzhui == "TIFF" || houzhui == "tif" ||
                houzhui == "TIF")
            {
                input_image_list.push_back(full_path);
            }
        }
    }
    closedir(p_dir);
#endif
    return;
}

smartmore::barcode::BarcodeType GetBarcodeDLType(int type)
{
    if (type == 2)
    {
        return smartmore::barcode::BarcodeType::kQrcode;
    }
    else if (type == 3)
    {
        return smartmore::barcode::BarcodeType::kDmcode;
    }
    else if (type == 1)
    {

        return smartmore::barcode::BarcodeType::kCode1D;
    }
    else
    {
        return smartmore::barcode::BarcodeType::kUnknown;
    }
}

void WriteCsvBuffer(std::ofstream &csv_file, const std::string image_path,
                    const std::array<cv::Point2f, 4> &boxes,
                    const std::string &result)
{
    const size_t pos = std::max<size_t>(image_path.find_last_of('/') + 1,
                                        image_path.find_last_of('\\') + 1);
    const std::string input_image_name =
        image_path.substr(pos, image_path.size());
    csv_file << input_image_name << ",\"";

    for (int i = 0; i < boxes.size(); ++i)
    {
        csv_file << boxes[i].x << ", " << boxes[i].y;
        if (!(i == boxes.size() - 1))
        {
            csv_file << ", ";
        }
    }

    csv_file << "\",\"" << result << "\"" << std::endl;
}

void WriteQualityBuffer(std::ofstream &csv_file, const std::string image_path,
                        const smartmore::barcode::ISOStandard &isostand,
                        const smartmore::barcode::BarcodeInfo &result,
                        bool add_tile)
{
    const int precision = 3;
    const size_t pos = std::max<size_t>(image_path.find_last_of('/') + 1,
                                        image_path.find_last_of('\\') + 1);
    const std::string input_image_name =
        image_path.substr(pos, image_path.size());

    if (isostand == smartmore::barcode::ISOStandard::kISO1541x)
    {
        if (add_tile)
        {
            csv_file
                << "name,overall,decode,sc,sc_value,mod,mod_value,an,an_"
                   "value,gn,gn_value,print_growth,pg_value_hori,pg_value_vert,"
                   "uec,uec_value,fpd,fpg_value,ppm\n";
        }

        auto iso = result.iso15415_quality;
        {
            csv_file << input_image_name << "," << iso.overall << ","
                     << iso.decode << "," << iso.sc << ","
                     << std::setprecision(precision) << iso.sc_value << ","
                     << iso.mod << "," << std::setprecision(precision)
                     << iso.mod_value << "," << iso.an << ","
                     << std::setprecision(precision) << iso.an_value << ","
                     << iso.gn << "," << std::setprecision(precision)
                     << iso.gn_value << "," << iso.print_growth << ","
                     << std::setprecision(precision)
                     << iso.print_growth_hori_value
                     << std::setprecision(precision) << ","
                     << iso.print_growth_vert_value << "," << iso.uec << ","
                     << std::setprecision(precision) << iso.uec_value << ","
                     << iso.fpd << "," << std::setprecision(precision)
                     << iso.fpd_value << "," << std::setprecision(precision)
                     << result.ppm << ","
                     << "\n";
        }
    }
    else if (isostand == smartmore::barcode::ISOStandard::kISO29158)
    {

        if (add_tile)
        {
            csv_file
                << "name,overall,decode,cc,cc_value,cm,cm_value,rt,rt_value,an,"
                   "an_value,gn,gn_value,print_growth,pg_value_hori,pg_value_"
                   "vert,uec,uec_value,ddg,ddg_value\n";
        }

        auto iso = result.iso29158_quality;
        {
            csv_file << input_image_name << "," << iso.overall << ","
                     << iso.decode << "," << iso.cc << ","
                     << std::setprecision(precision) << iso.cc_value << ","
                     << iso.cm << "," << std::setprecision(precision)
                     << iso.cm_value << "," << iso.r_target << ","
                     << std::setprecision(precision) << iso.r_target_value
                     << "," << iso.an << "," << std::setprecision(precision)
                     << iso.an_value << "," << iso.gn << ","
                     << std::setprecision(precision) << iso.gn_value << ","
                     << iso.print_growth << "," << std::setprecision(precision)
                     << iso.print_growth_hori_value
                     << std::setprecision(precision) << ","
                     << iso.print_growth_vert_value << "," << iso.uec << ","
                     << std::setprecision(precision) << iso.uec_value << ","
                     << iso.ddg << "," << std::setprecision(precision)
                     << iso.ddg_value << "," << std::setprecision(precision)
                     << result.ppm << ","
                     << "\n";
        }
    }
}

float TransformResize(cv::Mat &image, int height, int width)
{
    if (image.rows == height && image.cols == width)
    {
        return 1.0;
    }

    int image_height = image.rows;
    int image_width = image.cols;

    float height_ratio = height / static_cast<float>(image_height);
    float width_ratio = width / static_cast<float>(image_width);
    float ratio = std::min<float>(height_ratio, width_ratio);

    int resize_height = image_height * ratio;
    int resize_width = image_width * ratio;

    cv::resize(image, image, cv::Size(resize_width, resize_height), 0, 0,
               cv::INTER_LINEAR);

    cv::copyMakeBorder(image, image, 0, height - resize_height, 0,
                       width - resize_width, cv::BORDER_CONSTANT);

    return ratio;
}

std::string GetOnedTypeStr(smartmore::barcode::BarcodeType type)
{
    switch (type)
    {
    case smartmore::barcode::BarcodeType::kCode39:
        return "code39";
    case smartmore::barcode::BarcodeType::kCode93:
        return "code93";
    case smartmore::barcode::BarcodeType::kCode128:
        return "code128";
    case smartmore::barcode::BarcodeType::kEan8:
        return "ean8";
    case smartmore::barcode::BarcodeType::kEan13:
        return "ean13";
    case smartmore::barcode::BarcodeType::kItf:
        return "itf";
    case smartmore::barcode::BarcodeType::kUpcA:
        return "upca";
    case smartmore::barcode::BarcodeType::kUpcE:
        return "upce";
    case smartmore::barcode::BarcodeType::kPharmacode:
        return "pharmacode";
    default:
        return "oned";
    }
}

void WriteJsonFileBuffer(nlohmann::ordered_json &json,
                         const std::string &image_path,
                         smartmore::barcode::BarcodeResponse &output,
                         double time, bool status)
{
    json["image_path"] = image_path;
    json["succeed"] = status ? "true" : "false";
    json["run_time"] = time;

    json["results"] = nlohmann::ordered_json::array();
    for (int i = 0; i < output.results.size(); i++)
    {
        nlohmann::ordered_json result;
        smartmore::barcode::BarcodeInfo &info = output.results[i];

        result["succeed"] = info.succeed;
        result["decode_succeed"] = info.decode_succeed;
        result["detect_succeed"] = info.detect_succeed;

        // type
        if (info.type == smartmore::barcode::BarcodeType::kDmcode)
        {
            result["type"] = "dm";
        }
        else if (info.type == smartmore::barcode::BarcodeType::kQrcode)
        {
            result["type"] = "qr";
        }
        else if (info.type == smartmore::barcode::BarcodeType::kPdf417)
        {
            result["type"] = "pdf417";
        }
        else if (info.type == smartmore::barcode::BarcodeType::kUnknown)
        {
            result["type"] = "unknown";
        }
        else
        {
            result["type"] = GetOnedTypeStr(info.type);
        }
        result["result"] = info.result;

        // corner_boxes;
        result["corner_boxes"] = nlohmann::ordered_json::array();
        for (const auto &pt : info.corner_boxes)
        {
            result["corner_boxes"].push_back({pt.x, pt.y});
        }

        // coarse_locate_corner_boxes;
        result["coarse_locate_corner_boxes"] = nlohmann::ordered_json::array();
        for (const auto &pt : info.coarse_locate_corner_boxes)
        {
            result["coarse_locate_corner_boxes"].push_back({pt.x, pt.y});
        }

        // precise_locate_corner_boxes;
        result["precise_locate_corner_boxes"] = nlohmann::ordered_json::array();
        for (const auto &pt : info.precise_locate_corner_boxes)
        {
            result["precise_locate_corner_boxes"].push_back({pt.x, pt.y});
        }

        json["results"].push_back(result);
    }
}

void WriteJsonFileBuffer1D(nlohmann::ordered_json &json,
                           const std::string &image_path,
                           smartmore::barcode::BarcodeResponse &output,
                           int time, bool status)
{
    json["image_path"] = image_path;
    json["succeed"] = status ? "true" : "false";
    json["run_time"] = time / 1000.f;
    if (!status)
    {
        json["results"] = "null";
        return;
    }

    json["results"] = nlohmann::ordered_json::array();
    for (int i = 0; i < output.results.size(); i++)
    {
        nlohmann::ordered_json result;
        smartmore::barcode::BarcodeInfo &info = output.results[i];

        result["type"] = GetOnedTypeStr(info.type);

        result["result"] = info.result;

        result["rect_points"] = nlohmann::ordered_json::array();
        std::vector<cv::Point2f> pts(4);
        info.rect.points(pts.data());
        for (int j = 0; j < pts.size(); j++)
        {
            result["rect_points"].push_back({pts[j].x, pts[j].y});
        }

        json["results"].push_back(result);
    }
}

void WriteJsonFileBufferDL(nlohmann::ordered_json &json,
                           const std::string &image_path,
                           smartmore::barcode::BarcodeResponse &output,
                           int time, bool status)
{
    json["image_path"] = image_path;
    json["succeed"] = status ? "true" : "false";
    json["run_time"] = time / 1000.f;
    if (!status)
    {
        json["results"] = "null";
        return;
    }

    json["results"] = nlohmann::ordered_json::array();
    for (int i = 0; i < output.results.size(); i++)
    {
        if (output.results[i].succeed)
        {
            nlohmann::ordered_json result;
            smartmore::barcode::BarcodeInfo &info = output.results[i];

            if (info.type == smartmore::barcode::BarcodeType::kDmcode)
            {
                result["type"] = "dm";
            }
            else if (info.type == smartmore::barcode::BarcodeType::kQrcode)
            {
                result["type"] = "qr";
            }
            else
            {
                result["type"] = GetOnedTypeStr(info.type);
            }

            result["result"] = info.result;

            result["rect_points"] = nlohmann::ordered_json::array();
            std::vector<cv::Point2f> pts(4);
            info.rect.points(pts.data());
            if ((info.type == smartmore::barcode::BarcodeType::kQrcode) ||
                (info.type == smartmore::barcode::BarcodeType::kDmcode))
            {
                info.rect.points(pts.data());
            }
            else
            {
                // 一维码没有rect,只有corner
                auto &corner = info.corner_boxes;
                for (int k = 0; k < 4; k++)
                {
                    pts[k] = corner[k];
                }
            }
            for (int j = 0; j < pts.size(); j++)
            {
                result["rect_points"].push_back({pts[j].x, pts[j].y});
            }

            result["coarse_locate_corner_boxes"] =
                nlohmann::ordered_json::array();
            for (auto pt : info.coarse_locate_corner_boxes)
            {
                result["coarse_locate_corner_boxes"].push_back({pt.x, pt.y});
            }

            result["precise_locate_corner_boxes"] =
                nlohmann::ordered_json::array();
            for (auto pt : info.precise_locate_corner_boxes)
            {
                result["precise_locate_corner_boxes"].push_back({pt.x, pt.y});
            }

            json["results"].push_back(result);
        }
    }
}

std::string GetDateTime()
{
    struct tm tm;
    time_t tick = time(NULL);
    tm = *localtime(&tick);

    int year = tm.tm_year + 1900;
    int mon = tm.tm_mon + 1;
    int day = tm.tm_mday;
    int hours = tm.tm_hour;
    int min = tm.tm_min;
    int sec = tm.tm_sec;

    return std::to_string(year) + "-" + std::to_string(mon) + "-" +
           std::to_string(day) + "-" + std::to_string(hours) + "-" +
           std::to_string(min) + "-" + std::to_string(sec);
}

template <typename T>
std::ostream &operator<<(
    typename std::enable_if<std::is_enum<T>::value, std::ostream>::type &stream,
    const T &e)
{
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

void JudgeCodeType(const smartmore::barcode::BarcodeType &target_code_type,
                   const std::string &target_name,
                   const smartmore::barcode::BarcodeType &code_type,
                   std::vector<std::string> &output)
{
    if ((code_type & target_code_type) !=
        smartmore::barcode::BarcodeType::kUnknown)
    {
        output.push_back(target_name);
    }
}

std::ostream &operator<<(std::ostream &out,
                         const smartmore::barcode::BarcodeType &code_type)
{
    std::vector<std::string> output;
    JudgeCodeType(smartmore::barcode::BarcodeType::kCode39, "CODE_39",
                  code_type, output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kCode93, "CODE_93",
                  code_type, output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kCode128, "CODE_128",
                  code_type, output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kEan8, "EAN_8", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kEan13, "EAN_13", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kItf, "ITF", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kUpcA, "UPC_A", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kUpcE, "UPC_E", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kUpcEanExtension,
                  "UPC_EAN_EXTENSION", code_type, output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kQrcode, "QR", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kDmcode, "DM", code_type,
                  output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kPharmacode, "PHARMACODE",
                  code_type, output);
    JudgeCodeType(smartmore::barcode::BarcodeType::kPdf417, "PDF417", code_type,
                  output);

    const int output_size = output.size();
    if (output_size > 0)
    {
        for (int i = 0; i < output_size - 1; ++i)
        {
            out << output[i] << " | ";
        }
        out << output[output_size - 1];
    }
    else
    {
        out << "UNKNOWN";
    }

    return out;
}

void PrintResult(const smartmore::barcode::BarcodeInfo &result)
{
    std::cout << "idx: " << result.idx << ", ";
    std::cout << " code_type : " << result.type << ", ";

    std::cout << "result : " << result.result << std::endl;

    std::cout << "rect: " << result.rect.center << ", " << result.rect.size
              << ", " << result.rect.angle << std::endl;
    std::cout << "corner_boxes: " << result.corner_boxes[0] << ", "
              << result.corner_boxes[1] << ", " << result.corner_boxes[2]
              << ", " << result.corner_boxes[3] << std::endl;
    std::cout << "coarse_locate_corner_boxes: "
              << result.coarse_locate_corner_boxes[0] << ", "
              << result.coarse_locate_corner_boxes[1] << ", "
              << result.coarse_locate_corner_boxes[2] << ", "
              << result.coarse_locate_corner_boxes[3] << std::endl;
    std::cout << "precise_locate_corner_boxes: "
              << result.precise_locate_corner_boxes[0] << ", "
              << result.precise_locate_corner_boxes[1] << ", "
              << result.precise_locate_corner_boxes[2] << ", "
              << result.precise_locate_corner_boxes[3] << std::endl;
    std::cout << "center: " << result.center << ", ";
    std::cout << "angle degree: " << result.barcode_angle_deg << std::endl;
    std::cout << "info: {ppm: " << result.ppm << ", mirror: " << result.mirror
              << ", polarity: " << result.polarity
              << ", angle: " << result.barcode_angle_deg << ", version: ["
              << result.version[0] << ", " << result.version[1] << "]}"
              << std::endl;
    if (IsIntersect(result.type, smartmore::barcode::BarcodeType::kCode1D))
    {
        std::cout << "raw: " << result.raw << std::endl;
        int overall_grade = result.iso15416_quality.overall;
        std::cout << "quality analysis in grade: {overall: " << overall_grade
                  << ", rmin:" << result.iso15416_quality.rmin
                  << ", ecmin: " << result.iso15416_quality.ecmin
                  << ", sc: " << result.iso15416_quality.sc
                  << ", mod: " << result.iso15416_quality.mod
                  << ", defects: " << result.iso15416_quality.defects
                  << ", decodability: " << result.iso15416_quality.decodability
                  << "}" << std::endl;
    }
    else if (IsIntersect(result.type, smartmore::barcode::BarcodeType::kCode2D))
    {
        const int precision = 3;
        if (result.iso15415_quality.overall != -1)
        {
            std::cout << "ISO15415: {"
                      << "Overall: " << result.iso15415_quality.overall
                      << ", Decode: " << result.iso15415_quality.decode
                      << ", UEC: " << result.iso15415_quality.uec
                      << ", AN: " << result.iso15415_quality.an
                      << ", GN: " << result.iso15415_quality.gn
                      << ", SC: " << result.iso15415_quality.sc
                      << ", Mod: " << result.iso15415_quality.mod
                      << ", Print Growth: "
                      << result.iso15415_quality.print_growth
                      << ", FPD: " << result.iso15415_quality.fpd << "}"
                      << std::endl;

            std::cout << "ISO15415_Value: {"
                      << "UEC: " << std::setprecision(precision)
                      << result.iso15415_quality.uec_value
                      << ", AN: " << std::setprecision(precision)
                      << result.iso15415_quality.an_value
                      << ", GN: " << std::setprecision(precision)
                      << result.iso15415_quality.gn_value
                      << ", SC: " << std::setprecision(precision)
                      << result.iso15415_quality.sc_value
                      << ", Mod: " << std::setprecision(precision)
                      << result.iso15415_quality.mod_value
                      << ", HoriPrintGrowth: " << std::setprecision(precision)
                      << result.iso15415_quality.print_growth_hori_value
                      << ", VertPrintGrowth: " << std::setprecision(precision)
                      << result.iso15415_quality.print_growth_vert_value
                      << ", FPD: " << std::setprecision(precision)
                      << result.iso15415_quality.fpd_value << "}" << std::endl;
        }

        if (result.iso29158_quality.overall != -1)
        {
            std::cout << "ISO29158: {"
                      << "Overall: " << result.iso29158_quality.overall
                      << ", Decode: " << result.iso29158_quality.decode
                      << ", UEC: " << result.iso29158_quality.uec
                      << ", AN: " << result.iso29158_quality.an
                      << ", GN: " << result.iso29158_quality.gn
                      << ", Rt: " << result.iso29158_quality.r_target
                      << ", CC: " << result.iso29158_quality.cc
                      << ", CM: " << result.iso29158_quality.cm
                      << ", Print Growth:"
                      << result.iso29158_quality.print_growth
                      << ", DDG: " << result.iso29158_quality.ddg << "}"
                      << std::endl;

            std::cout << "ISO29158_value: {"
                      << "UEC: " << std::setprecision(precision)
                      << result.iso29158_quality.uec_value
                      << ", AN: " << std::setprecision(precision)
                      << result.iso29158_quality.an_value
                      << ", GN: " << std::setprecision(precision)
                      << result.iso29158_quality.gn_value
                      << ", Rt: " << std::setprecision(precision)
                      << result.iso29158_quality.r_target_value
                      << ", CC: " << std::setprecision(precision)
                      << result.iso29158_quality.cc_value
                      << ", CM: " << std::setprecision(precision)
                      << result.iso29158_quality.cm_value
                      << ", HoriPrintGrowth: " << std::setprecision(precision)
                      << result.iso29158_quality.print_growth_hori_value
                      << ", VertPrintGrowth: " << std::setprecision(precision)
                      << result.iso29158_quality.print_growth_vert_value
                      << ", DDG: " << std::setprecision(precision)
                      << result.iso29158_quality.ddg_value << "}" << std::endl;
        }
    }
}

bool DiffResult(const smartmore::barcode::BarcodeInfo &result_1,
                const smartmore::barcode::BarcodeInfo &result_2)
{
    return 0 == result_1.result.compare(result_2.result);
}

bool CompareBarcodeResponse(const smartmore::barcode::BarcodeResponse &res_1,
                            const smartmore::barcode::BarcodeResponse &res_2,
                            int img_height, int img_width)
{
    for (int res_idx = 0; res_idx < res_1.results.size(); ++res_idx)
    {
        // boundary check for image
        std::array<cv::Point2f, 4> coarse_loc =
            res_1.results[res_idx].coarse_locate_corner_boxes;
        float x_min = coarse_loc[0].x, x_max = coarse_loc[0].x;
        float y_min = coarse_loc[0].y, y_max = coarse_loc[0].y;
        for (const auto &point : coarse_loc)
        {
            x_min = std::min(x_min, point.x);
            x_max = std::max(x_max, point.x);
            y_min = std::min(y_min, point.y);
            y_max = std::max(y_max, point.y);
        }
        if (x_min < 0 || x_max > img_width || y_min < 0 || y_max > img_height)
        {
            continue;
        }
        if (res_1.results[res_idx].succeed ^ res_2.results[res_idx].succeed)
        {
            std::cerr << "The exception of different succeed flag "
                         "using roi!"
                      << std::endl;
            return false;
        }
        if (res_1.results[res_idx].type != res_2.results[res_idx].type)
        {
            std::cerr << "The exception of different type using roi!"
                      << std::endl;
            return false;
        }
        if (!DiffResult(res_1.results[res_idx], res_2.results[res_idx]))
        {
            std::cerr << "The exception of different result "
                         "using roi!"
                      << std::endl;
            return false;
        }
    }
    return true;
}

smartmore::barcode::BarcodeType StringToType(std::string type_string)
{
    std::map<std::string, smartmore::barcode::BarcodeType> string_to_format_map;
    string_to_format_map["CODE_39"] = smartmore::barcode::BarcodeType::kCode39;
    string_to_format_map["CODE_93"] = smartmore::barcode::BarcodeType::kCode93;
    string_to_format_map["CODE_128"] =
        smartmore::barcode::BarcodeType::kCode128;
    string_to_format_map["EAN_8"] = smartmore::barcode::BarcodeType::kEan8;
    string_to_format_map["EAN_13"] = smartmore::barcode::BarcodeType::kEan13;
    string_to_format_map["ITF"] = smartmore::barcode::BarcodeType::kItf;
    string_to_format_map["UPC_A"] = smartmore::barcode::BarcodeType::kUpcA;
    string_to_format_map["UPC_E"] = smartmore::barcode::BarcodeType::kUpcE;
    string_to_format_map["UPC_EAN_EXTENSION"] =
        smartmore::barcode::BarcodeType::kUpcEanExtension;
    string_to_format_map["QR"] = smartmore::barcode::BarcodeType::kQrcode;
    string_to_format_map["qr"] = smartmore::barcode::BarcodeType::kQrcode;
    string_to_format_map["DM"] = smartmore::barcode::BarcodeType::kDmcode;
    string_to_format_map["dm"] = smartmore::barcode::BarcodeType::kDmcode;
    string_to_format_map["UNKNOWN"] = smartmore::barcode::BarcodeType::kUnknown;
    string_to_format_map["1D"] = smartmore::barcode::BarcodeType::kCode1D;
    string_to_format_map["2D"] = smartmore::barcode::BarcodeType::kCode2D;
    string_to_format_map["Pharmacode"] =
        smartmore::barcode::BarcodeType::kPharmacode;
    string_to_format_map["Pdf417"] = smartmore::barcode::BarcodeType::kPdf417;

    if (string_to_format_map.find(type_string) == string_to_format_map.end())
    {
        std::cout
            << "unknown barcode1d type，type can "
               "be:\nCODE_39\nCODE_93\nCODE_128\nEAN_8\nEAN_13\nITF"
               "\nUPC_A\nUPC_E\nUPC_EAN_EXTENSION\nQR\nDM\n1D\n2D\nPharmacode\n"
            << std::endl;
        abort();
    }
    else
    {
        return string_to_format_map[type_string];
    }
}

smartmore::barcode::ISOStandard StringToISOStand(std::string type_string)
{
    std::map<std::string, smartmore::barcode::ISOStandard> string_to_format_map;
    string_to_format_map["iso1541x"] =
        smartmore::barcode::ISOStandard::kISO1541x;
    string_to_format_map["iso29158"] =
        smartmore::barcode::ISOStandard::kISO29158;

    if (string_to_format_map.find(type_string) == string_to_format_map.end())
    {
        std::cout << "unknown isostand type，type can "
                     "be:\niso1541x\niso29158\n"
                  << std::endl;
        abort();
    }
    else
    {
        return string_to_format_map[type_string];
    }
}

std::string GetSignatureFromFilename(std::string file_name)
{
    std::regex reg("/");
    std::sregex_token_iterator pos(file_name.begin(), file_name.end(), reg, -1);
    decltype(pos) end;
    std::vector<std::string> items;
    for (; pos != end; ++pos)
    {
        items.emplace_back(pos->str());
    }
    if (items.size() < 2)
    {
        std::cout << "can't calculate signature from " << file_name
                  << std::endl;
        return "";
    }
    std::string json_name = items.back();
    std::string signature =
        std::regex_replace(json_name, std::regex(".json"), "");
    signature = std::regex_replace(signature, std::regex("config_"), "");
    signature = std::regex_replace(signature, std::regex("dl_"), "");
    std::string directory = *(items.end() - 2);
    if (directory == "nxp_dl")
    {
        signature = "nxp" + signature;
    }
    else if (directory == "hailo")
    {
        signature = "hailo" + signature;
    }
    return signature;
}

#if TARGET_OS_IPHONE
#import <Foundation/Foundation.h>

std::string GeneratePathForIOS(const std::string relative_path)
{
    @autoreleasepool
    {
        NSString *mainBundlePath = [[NSBundle mainBundle] bundlePath];
        return std::move(std::string([mainBundlePath UTF8String]) + "/" +
                         relative_path);
    }
};

std::string GenerateOutputPath(const std::string relative_path)
{
    @autoreleasepool
    {
        NSString *prefixPath;
        NSString *docPath = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES)[0];
        NSString *resultPath = [NSString
            stringWithFormat:@"%@/%s", docPath, relative_path.c_str()];
        NSString *parentOfResultPath =
            [resultPath stringByDeletingLastPathComponent];

        NSFileManager *fileManager = [NSFileManager defaultManager];
        if ([fileManager fileExistsAtPath:parentOfResultPath])
        {
            std::cout << [parentOfResultPath UTF8String] << " exists!"
                      << std::endl;
        }
        else if ([fileManager createDirectoryAtPath:parentOfResultPath
                        withIntermediateDirectories:YES
                                         attributes:nil
                                              error:nil])
        {
            std::cout << [parentOfResultPath UTF8String] << " created!"
                      << std::endl;
        }
        else
        {
            std::cout << "Fail to create " << [parentOfResultPath UTF8String]
                      << std::endl;
        }
        return [resultPath UTF8String];
    }
};
#endif

template <class ReadT, class WriteT>
void WriteMatToFile(const std::string &filename, const cv::Mat &m)
{
    std::cout << "save mat with shape: " << m.size() << " to: " << filename
              << std::endl;

    std::ofstream file_out(filename);

    if (!file_out)
    {
        file_out << "File Not Opened" << std::endl;
        return;
    }

    for (int i = 0; i < m.rows; i++)
    {
        for (int j = 0; j < m.cols; j++)
        {
            file_out << static_cast<WriteT>(m.at<ReadT>(i, j)) << " ";
        }
        file_out << std::endl;
    }

    file_out.close();
}

inline void SaveMatToFile(std::string path, const cv::Mat &mat)
{
    switch (mat.depth())
    {
    case (CV_8S):
        WriteMatToFile<int8_t, int>(path, mat);
        break;
    case (CV_8U):
        WriteMatToFile<uint8_t, int>(path, mat);
        break;
    case (CV_16S):
        WriteMatToFile<int16_t, int>(path, mat);
        break;
    case (CV_16U):
        WriteMatToFile<uint16_t, int>(path, mat);
        break;
    case (CV_32F):
        WriteMatToFile<float, float>(path, mat);
        break;
    case (CV_64F):
        WriteMatToFile<double, float>(path, mat);
        break;
    }
};

inline void DrawRect(cv::Mat &canvas, std::array<cv::Point2f, 4> rect,
                     cv::Scalar color, int thickness = 1)
{
    for (int i = 0; i < 4; i++)
    {
        cv::line(canvas, rect[i], rect[(i + 1) % 4], color, thickness);
    }
};

#ifdef WITH_NOVAIC

// release vs600 npu mem
void NovaicCommomUninit() { CoManager::CommomUninit(); }

// init vs600 npu mem
void NovaicInitComMem() { InitComMem(); }

#endif

#endif // __SMARTMORE_BARCODE_COMMON_H__
