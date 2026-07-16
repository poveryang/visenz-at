#include "at_orchestrator.h"
#include "at_types.h"

#include "camcap/camcap.h"
#include "camcap/opencv_adapter.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr int kDefaultPort = 8080;
constexpr std::uint32_t kMaxCommandBytes = 64 * 1024;

struct Options {
    std::string device{"vs1000p_2mp"};
    int steps{8};
    int port{kDefaultPort};
    camcap::CameraParams initial_params{};
    std::filesystem::path output_dir{};
    bool save_images{false};
    bool server{false};
    std::string heatmap_model;
    std::string heatmap_context{"timvx"};
    std::string heatmap_precision{"uint8"};
    bool heatmap_overlay{true};
    double heatmap_threshold{0.25};
};

struct Response {
    bool ok{true};
    camcap::Error error{};
    camcap::Status status{};
    std::string trace_json;
    std::string at_json;
    std::string image_encoding;
    std::vector<unsigned char> image;
};

// Timings are deliberately taken around the device-facing calls.  Heatmap/model
// timings remain in heatmap_perf, while these fields identify lens, capture and
// optional image-export stalls separately.
struct StepTiming {
    double set_params_ms{-1.0};
    double capture_ms{-1.0};
    double at_core_ms{-1.0};
    double image_save_ms{-1.0};
    double total_ms{-1.0};
};

struct AsyncRunState {
    mutable std::mutex mutex;
    std::atomic_bool stop_requested{false};
    bool running{false};
    bool finished{false};
    int max_steps{0};
    int completed_steps{0};
    std::string finish_reason{"Idle"};
    std::string last_trace_json;
    int preview_version{0};
    cv::Mat latest_preview;  // Raw frame only: encoding is deferred to get_preview.
    std::thread worker;
};

std::string EscapeJson(const std::string &text)
{
    std::ostringstream out;
    for (const char c : text) {
        switch (c) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << c;
                break;
        }
    }
    return out.str();
}

std::array<int, 4> ParseLights(const std::string &text)
{
    std::array<int, 4> lights{};
    std::stringstream ss(text);
    std::string token;
    std::size_t index = 0;
    while (std::getline(ss, token, ',')) {
        if (index >= lights.size()) {
            throw std::runtime_error("--lights expects four comma-separated integers");
        }
        lights[index++] = std::stoi(token);
    }
    if (index != lights.size()) {
        throw std::runtime_error("--lights expects four comma-separated integers");
    }
    return lights;
}

std::string Trim(std::string text)
{
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [&](char c) {
                   return !is_space(static_cast<unsigned char>(c));
               }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [&](char c) {
                   return !is_space(static_cast<unsigned char>(c));
               }).base(), text.end());
    return text;
}

std::optional<std::string> FindString(const std::string &json, const std::string &key)
{
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = json.find(quoted_key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = json.find(':', key_pos + quoted_key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const auto first_quote = json.find('"', colon + 1);
    if (first_quote == std::string::npos) {
        return std::nullopt;
    }
    const auto second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return std::nullopt;
    }
    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

std::optional<int> FindInt(const std::string &json, const std::string &key)
{
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = json.find(quoted_key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = json.find(':', key_pos + quoted_key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    auto begin = colon + 1;
    while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin])) != 0) {
        ++begin;
    }
    auto end = begin;
    if (end < json.size() && json[end] == '-') {
        ++end;
    }
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])) != 0) {
        ++end;
    }
    if (end == begin) {
        return std::nullopt;
    }
    return std::stoi(json.substr(begin, end - begin));
}

std::optional<bool> FindBool(const std::string &json, const std::string &key)
{
    const std::string quoted_key = "\"" + key + "\"";
    const auto key_pos = json.find(quoted_key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = json.find(':', key_pos + quoted_key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    auto begin = colon + 1;
    while (begin < json.size() && std::isspace(static_cast<unsigned char>(json[begin])) != 0) {
        ++begin;
    }
    if (json.compare(begin, 4, "true") == 0) {
        return true;
    }
    if (json.compare(begin, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

std::optional<std::array<int, 4>> FindLights(const std::string &json)
{
    const std::string quoted_key = "\"lights\"";
    const auto key_pos = json.find(quoted_key);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const auto begin = json.find('[', key_pos + quoted_key.size());
    const auto end = json.find(']', begin == std::string::npos ? key_pos : begin);
    if (begin == std::string::npos || end == std::string::npos || end <= begin) {
        return std::nullopt;
    }

    std::array<int, 4> lights{};
    std::stringstream ss(json.substr(begin + 1, end - begin - 1));
    std::string token;
    std::size_t index = 0;
    while (std::getline(ss, token, ',')) {
        if (index >= lights.size()) {
            return std::nullopt;
        }
        lights[index++] = std::stoi(Trim(token));
    }
    if (index != lights.size()) {
        return std::nullopt;
    }
    return lights;
}

std::optional<camcap::CameraParams> ParseParams(const std::string &json)
{
    auto exposure = FindInt(json, "exposure_us");
    if (!exposure) {
        exposure = FindInt(json, "exposure");
    }
    const auto gain = FindInt(json, "gain");
    const auto focus = FindInt(json, "focus");
    const auto lights = FindLights(json);
    if (!exposure || !gain || !focus || !lights) {
        return std::nullopt;
    }

    camcap::CameraParams params;
    params.exposure_us = *exposure;
    params.gain = *gain;
    params.focus = *focus;
    params.lights = *lights;
    return params;
}

void PrintUsage(const char *program)
{
    std::cerr << "Usage: " << program
              << " [--device vs1000p_2mp] [--steps 8] [--exposure-us 1000]"
              << " [--gain 50] [--focus 30] [--lights 1,1,1,1]"
              << " [--out-dir /tmp/at_run] [--save-images]"
              << " [--heatmap-model /path/yolo-uint8.tmfile]\n"
              << "       " << program
              << " --server [--device vs1000p_2mp] [--port 8080]"
              << " [--exposure-us 1000] [--gain 50] [--focus 30] [--lights 1,1,1,1]"
              << " [--heatmap-model /path/yolo-uint8.tmfile]\n"
              << "Detect options: [--heatmap-context timvx|cpu] [--heatmap-precision uint8|fp32]"
              << " [--heatmap-threshold 0.25] [--no-heatmap-overlay]\n";
}

Options ParseArgs(int argc, char **argv)
{
    Options options;
    options.initial_params.exposure_us = 1000;
    options.initial_params.gain = 50;
    options.initial_params.focus = 30;
    options.initial_params.lights = {1, 1, 1, 1};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char *name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--device") {
            options.device = require_value("--device");
        } else if (arg == "--steps") {
            options.steps = std::max(1, std::stoi(require_value("--steps")));
        } else if (arg == "--port") {
            options.port = std::stoi(require_value("--port"));
        } else if (arg == "--exposure-us") {
            options.initial_params.exposure_us = std::stoi(require_value("--exposure-us"));
        } else if (arg == "--gain") {
            options.initial_params.gain = std::stoi(require_value("--gain"));
        } else if (arg == "--focus") {
            options.initial_params.focus = std::stoi(require_value("--focus"));
        } else if (arg == "--lights") {
            options.initial_params.lights = ParseLights(require_value("--lights"));
        } else if (arg == "--out-dir") {
            options.output_dir = require_value("--out-dir");
        } else if (arg == "--save-images") {
            options.save_images = true;
        } else if (arg == "--server") {
            options.server = true;
        } else if (arg == "--heatmap-model") {
            options.heatmap_model = require_value("--heatmap-model");
        } else if (arg == "--heatmap-context") {
            options.heatmap_context = require_value("--heatmap-context");
        } else if (arg == "--heatmap-precision") {
            options.heatmap_precision = require_value("--heatmap-precision");
        } else if (arg == "--heatmap-threshold") {
            options.heatmap_threshold = std::stod(require_value("--heatmap-threshold"));
        } else if (arg == "--no-heatmap-overlay") {
            options.heatmap_overlay = false;
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    return options;
}

at::SessionConfig MakeSessionConfig(const Options &options)
{
    at::SessionConfig config;
    // VS1000P 2MP 镜头的可用机械焦点行程约为 [0, 420]。AT 的通用默认值
    // 覆盖更宽的设备范围；runner 必须在设备边界处收紧，避免粗扫/精扫产生
    // 无效的大行程移动。
    if (options.device == "vs1000p_2mp" || options.device == "vs1000p_5mp") {
        config.camera.min_focus_pos = 0;
        config.camera.max_focus_pos = 420;
        // VS1000P 的有效照明组合限定为三组。避免关灯/单灯导致无码或
        // 低亮度帧挤占流程预算，也保持现场可复现的灯光切换顺序。
        config.flow.light_profiles = {
            {1, 1, 1, 1},
            {1, 1, 0, 0},
            {0, 0, 1, 1},
        };
    }
    config.heatmap.model_path = options.heatmap_model;
    config.heatmap.context = options.heatmap_context;
    config.heatmap.precision = options.heatmap_precision;
    config.heatmap.threshold = options.heatmap_threshold;
    config.heatmap.overlay = options.heatmap_overlay;
    return config;
}

at::CameraParams ToAtParams(const camcap::CameraParams &params)
{
    at::CameraParams at_params;
    at_params.exp_time = params.exposure_us;
    at_params.exp_gain = params.gain;
    at_params.focus_pos = params.focus;
    at_params.lights.assign(params.lights.begin(), params.lights.end());
    return at_params;
}

camcap::CameraParams ToCamcapParams(const at::CameraParams &params,
                                    const camcap::CameraParams &fallback)
{
    camcap::CameraParams out;
    out.exposure_us = params.exp_time;
    out.gain = params.exp_gain;
    out.focus = params.focus_pos;
    out.lights = fallback.lights;
    for (std::size_t i = 0; i < out.lights.size() && i < params.lights.size(); ++i) {
        out.lights[i] = params.lights[i];
    }
    return out;
}

std::string CamcapParamsJson(const camcap::CameraParams &params)
{
    std::ostringstream out;
    out << "{\"exposure_us\":" << params.exposure_us << ",\"gain\":" << params.gain
        << ",\"focus\":" << params.focus << ",\"lights\":[" << params.lights[0] << ','
        << params.lights[1] << ',' << params.lights[2] << ',' << params.lights[3] << "]}";
    return out.str();
}

std::string ParamsJson(const at::CameraParams &params)
{
    std::ostringstream out;
    out << "{\"exposure_us\":" << params.exp_time << ",\"gain\":" << params.exp_gain
        << ",\"focus\":" << params.focus_pos << ",\"lights\":[";
    for (std::size_t i = 0; i < params.lights.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << params.lights[i];
    }
    out << "]}";
    return out.str();
}

std::string QualityJson(const at::ImageQuality &quality)
{
    std::ostringstream out;
    out << "{\"brightness\":" << quality.brightness << ",\"contrast\":" << quality.contrast
        << ",\"sharpness\":" << quality.sharpness << ",\"saturation_ratio\":"
        << quality.saturation_ratio << ",\"noise_proxy\":" << quality.noise_proxy << "}";
    return out.str();
}

std::string RectJson(const cv::Rect &rect)
{
    std::ostringstream out;
    out << "{\"x\":" << rect.x << ",\"y\":" << rect.y
        << ",\"width\":" << rect.width << ",\"height\":" << rect.height << "}";
    return out.str();
}

std::string HeatmapJson(const at::HeatmapObservation &heatmap)
{
    std::ostringstream out;
    out << "{\"available\":" << (heatmap.available ? "true" : "false")
        << ",\"roi\":" << RectJson(heatmap.roi)
        << ",\"confidence\":" << heatmap.confidence
        << ",\"feature_strength\":" << heatmap.feature_strength
        << ",\"stability\":" << heatmap.stability
        << ",\"source\":\"" << EscapeJson(heatmap.source)
        << "\",\"model_version\":\"" << EscapeJson(heatmap.model_version) << "\"}";
    return out.str();
}

std::string TraceJson(const at::StepResult &result,
                      const at::CameraParams &current_params,
                      const std::string &image_path,
                      const at::AtOrchestrator *orchestrator,
                      const StepTiming *timing = nullptr)
{
    std::ostringstream out;
    out << "{\"step\":" << result.trace.step_index << ",\"phase\":\""
        << at::ToString(result.trace.phase) << "\",\"action\":\""
        << at::ToString(result.trace.action) << "\",\"need_decode\":"
        << (result.need_decode ? "true" : "false") << ",\"finished\":"
        << (result.finished ? "true" : "false") << ",\"finish_reason\":\""
        << at::ToString(result.trace.finish_reason) << "\",\"stage_step_count\":"
        << result.trace.stage_step_count << ",\"decode_used\":"
        << result.trace.decode_used
        << ",\"current_params\":" << ParamsJson(current_params)
        << ",\"next_params\":" << ParamsJson(result.next_params)
        << ",\"quality\":" << QualityJson(result.trace.quality)
        << ",\"heatmap\":" << HeatmapJson(result.trace.heatmap)
        << ",\"reason\":\"" << EscapeJson(result.trace.reason) << "\"";
    if (orchestrator != nullptr) {
        const std::string perf = orchestrator->HeatmapPerfJson();
        if (perf != "null") {
            out << ",\"heatmap_perf\":" << perf;
        }
    }
    if (!image_path.empty()) {
        out << ",\"image_path\":\"" << EscapeJson(image_path) << "\"";
    }
    if (timing != nullptr) {
        if (timing->set_params_ms >= 0.0) out << ",\"device_set_params_ms\":" << timing->set_params_ms;
        if (timing->capture_ms >= 0.0) out << ",\"device_capture_ms\":" << timing->capture_ms;
        if (timing->at_core_ms >= 0.0) out << ",\"device_at_core_ms\":" << timing->at_core_ms;
        if (timing->image_save_ms >= 0.0) out << ",\"device_image_save_ms\":" << timing->image_save_ms;
        if (timing->at_core_ms >= 0.0) out << ",\"device_core_ms\":" << (timing->set_params_ms + timing->capture_ms + timing->at_core_ms);
        if (timing->total_ms >= 0.0) out << ",\"device_total_ms\":" << timing->total_ms;
    }
    out << "}";
    return out.str();
}

std::string HeatmapCaptureTraceJson(const at::HeatmapObservation &heatmap,
                                    const at::CameraParams &current_params,
                                    const at::AtOrchestrator *orchestrator)
{
    std::ostringstream out;
    out << "{\"event\":\"capture_heatmap\""
        << ",\"current_params\":" << ParamsJson(current_params)
        << ",\"heatmap\":" << HeatmapJson(heatmap);
    if (orchestrator != nullptr) {
        const std::string perf = orchestrator->HeatmapPerfJson();
        if (perf != "null") {
            out << ",\"heatmap_perf\":" << perf;
        }
    }
    out << "}";
    return out.str();
}

std::string StatusJson(const camcap::Status &status)
{
    std::ostringstream out;
    out << "{\"device\":\"" << EscapeJson(status.device) << "\",\"backend\":\""
        << camcap::toString(status.backend) << "\",\"opened\":" << (status.opened ? "true" : "false")
        << ",\"width\":" << status.width << ",\"height\":" << status.height
        << ",\"pixel_format\":\"" << camcap::toString(status.format) << "\",\"params\":"
        << CamcapParamsJson(status.params) << ",\"last_error\":{\"code\":\""
        << camcap::toString(status.last_error.code) << "\",\"message\":\""
        << EscapeJson(status.last_error.message) << "\"}}";
    return out.str();
}

std::string ResponseJson(const Response &response)
{
    std::ostringstream out;
    out << "{\"ok\":" << (response.ok ? "true" : "false") << ",\"error\":{\"code\":\""
        << camcap::toString(response.error.code) << "\",\"message\":\""
        << EscapeJson(response.error.message) << "\"},\"status\":" << StatusJson(response.status)
        << ",\"image\":{\"encoding\":\"" << EscapeJson(response.image_encoding)
        << "\",\"size\":" << response.image.size() << "}";
    if (!response.at_json.empty()) {
        out << ",\"at\":" << response.at_json;
    }
    if (!response.trace_json.empty()) {
        out << ",\"trace\":" << response.trace_json;
    }
    out << "}";
    return out.str();
}

bool RecvAll(const int fd, void *data, const std::size_t size)
{
    auto *cursor = static_cast<char *>(data);
    std::size_t received = 0;
    while (received < size) {
        const ssize_t ret = recv(fd, cursor + received, size - received, MSG_WAITALL);
        if (ret <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(ret);
    }
    return true;
}

bool SendAll(const int fd, const void *data, const std::size_t size)
{
    const auto *cursor = static_cast<const char *>(data);
    std::size_t sent = 0;
    while (sent < size) {
        const ssize_t ret = send(fd, cursor + sent, size - sent, 0);
        if (ret <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(ret);
    }
    return true;
}

std::optional<std::string> ReceiveCommand(const int client_fd)
{
    std::uint32_t network_size = 0;
    if (!RecvAll(client_fd, &network_size, sizeof(network_size))) {
        return std::nullopt;
    }

    const std::uint32_t size = ntohl(network_size);
    if (size == 0 || size > kMaxCommandBytes) {
        return std::nullopt;
    }

    std::string command(size, '\0');
    if (!RecvAll(client_fd, command.data(), command.size())) {
        return std::nullopt;
    }
    return command;
}

bool SendResponse(const int client_fd, const Response &response)
{
    const std::string json = ResponseJson(response);
    const std::uint32_t network_size = htonl(static_cast<std::uint32_t>(json.size()));
    return SendAll(client_fd, &network_size, sizeof(network_size)) &&
           SendAll(client_fd, json.data(), json.size()) &&
           (response.image.empty() || SendAll(client_fd, response.image.data(), response.image.size()));
}

Response OkResponse(const camcap::Camera &camera)
{
    Response response;
    response.ok = true;
    response.status = camera.status();
    return response;
}

Response ErrorResponse(const camcap::Camera &camera, camcap::Error error)
{
    Response response;
    response.ok = false;
    response.error = std::move(error);
    response.status = camera.status();
    return response;
}

camcap::Result<cv::Mat> CaptureMat(camcap::Camera &camera)
{
    auto frame = camera.capture();
    if (!frame) {
        return camcap::Result<cv::Mat>::fail(frame.error());
    }
    return camcap::toCvMat(frame.value());
}

bool EncodePng(const cv::Mat &mat, std::vector<unsigned char> &payload)
{
    payload.clear();
    return cv::imencode(".png", mat, payload);
}

Response CaptureResponse(camcap::Camera &camera)
{
    auto mat = CaptureMat(camera);
    if (!mat) {
        return ErrorResponse(camera, mat.error());
    }

    Response response = OkResponse(camera);
    response.image_encoding = "png";
    if (!EncodePng(mat.value(), response.image)) {
        return ErrorResponse(camera, camcap::makeError(camcap::ErrorCode::EncodeFailed,
                                                       "failed to encode png"));
    }
    return response;
}

Response CaptureHeatmapResponse(camcap::Camera &camera,
                                at::AtOrchestrator &orchestrator,
                                const camcap::CameraParams &current_params,
                                const bool overlay)
{
    auto mat = CaptureMat(camera);
    if (!mat) {
        return ErrorResponse(camera, mat.error());
    }

    at::FrameContext input;
    input.image = mat.value();
    input.current_params = ToAtParams(current_params);
    const at::HeatmapObservation heatmap = orchestrator.ObserveHeatmap(input);

    Response response = OkResponse(camera);
    response.image_encoding = "png";
    const cv::Mat image = overlay ? orchestrator.BlendForDisplay(mat.value()) : mat.value();
    if (!EncodePng(image, response.image)) {
        return ErrorResponse(camera, camcap::makeError(camcap::ErrorCode::EncodeFailed,
                                                       "failed to encode png"));
    }
    response.trace_json = HeatmapCaptureTraceJson(heatmap, input.current_params, &orchestrator);
    return response;
}

Response AtStepResponse(camcap::Camera &camera,
                        at::AtOrchestrator &orchestrator,
                        camcap::CameraParams &current_params)
{
    if (auto set = camera.setParams(current_params); !set) {
        return ErrorResponse(camera, set.error());
    }

    auto mat = CaptureMat(camera);
    if (!mat) {
        return ErrorResponse(camera, mat.error());
    }

    at::FrameContext input;
    input.image = mat.value();
    input.current_params = ToAtParams(current_params);

    const at::StepResult result = orchestrator.ProcessStep(input);
    current_params = ToCamcapParams(result.next_params, current_params);
    if (auto set = camera.setParams(current_params); !set) {
        return ErrorResponse(camera, set.error());
    }

    Response response = OkResponse(camera);
    response.image_encoding = "png";
    const cv::Mat display_image = orchestrator.BlendForDisplay(mat.value());
    if (!EncodePng(display_image, response.image)) {
        return ErrorResponse(camera, camcap::makeError(camcap::ErrorCode::EncodeFailed,
                                                       "failed to encode png"));
    }
    response.trace_json = TraceJson(result, input.current_params, "", &orchestrator);
    response.at_json = std::string("{\"finished\":") + (result.finished ? "true" : "false") +
                       ",\"need_decode\":" + (result.need_decode ? "true" : "false") +
                       ",\"step\":" + std::to_string(result.trace.step_index) + "}";
    return response;
}

int RunBatch(const Options &options)
{
    at::AtOrchestrator orchestrator(MakeSessionConfig(options));

    if (options.save_images || !options.output_dir.empty()) {
        std::filesystem::path output_dir = options.output_dir.empty() ? "at_device_run" : options.output_dir;
        std::filesystem::create_directories(output_dir);
    }

    std::ofstream trace_file;
    std::filesystem::path output_dir = options.output_dir;
    if (options.save_images && output_dir.empty()) {
        output_dir = "at_device_run";
    }
    if (!output_dir.empty()) {
        const auto trace_path = output_dir / "trace.jsonl";
        trace_file.open(trace_path, std::ios::out | std::ios::trunc);
        if (!trace_file) {
            std::cerr << "failed to open trace file: " << trace_path << '\n';
            return 1;
        }
    }

    camcap::DeviceConfig device_config;
    device_config.device = options.device;
    camcap::Camera camera(device_config);
    std::cerr << "camera open start: device=" << options.device << '\n';
    if (auto opened = camera.open(); !opened) {
        std::cerr << "open camera failed: " << opened.error().message << '\n';
        return 1;
    }
    std::cerr << "camera open done\n";

    camcap::CameraParams current = options.initial_params;

    for (int step = 0; step < options.steps; ++step) {
        const auto step_start = std::chrono::steady_clock::now();
        if (auto set = camera.setParams(current); !set) {
            std::cerr << "set params failed: " << set.error().message << '\n';
            return 1;
        }
        const auto set_end = std::chrono::steady_clock::now();

        auto mat = CaptureMat(camera);
        if (!mat) {
            std::cerr << "capture failed: " << mat.error().message << '\n';
            return 1;
        }
        const auto capture_end = std::chrono::steady_clock::now();

        at::FrameContext input;
        input.image = mat.value();
        input.current_params = ToAtParams(current);

        const at::StepResult result = orchestrator.ProcessStep(input);
        const auto at_core_end = std::chrono::steady_clock::now();
        std::string image_path;
        if (options.save_images) {
            image_path = (output_dir / ("step_" + std::to_string(step) + ".png")).string();
            const cv::Mat display_image = orchestrator.BlendForDisplay(mat.value());
            if (!cv::imwrite(image_path, display_image)) {
                std::cerr << "failed to save image: " << image_path << '\n';
                return 1;
            }
        }
        const auto step_end = std::chrono::steady_clock::now();
        StepTiming timing;
        timing.set_params_ms = std::chrono::duration<double, std::milli>(set_end - step_start).count();
        timing.capture_ms = std::chrono::duration<double, std::milli>(capture_end - set_end).count();
        timing.at_core_ms = std::chrono::duration<double, std::milli>(at_core_end - capture_end).count();
        timing.image_save_ms = std::chrono::duration<double, std::milli>(step_end - at_core_end).count();
        timing.total_ms = std::chrono::duration<double, std::milli>(step_end - step_start).count();

        const std::string trace_json = TraceJson(result,
                                                 input.current_params,
                                                 image_path,
                                                 &orchestrator,
                                                 &timing);
        std::cout << trace_json << std::endl;
        if (trace_file.is_open()) {
            trace_file << trace_json << '\n';
        }

        if (result.finished) {
            break;
        }
        current = ToCamcapParams(result.next_params, current);
    }

    return 0;
}

std::string AsyncRunJson(const AsyncRunState &state)
{
    std::lock_guard<std::mutex> lock(state.mutex);
    std::ostringstream out;
    out << "{\"running\":" << (state.running ? "true" : "false")
        << ",\"finished\":" << (state.finished ? "true" : "false")
        << ",\"completed_steps\":" << state.completed_steps
        << ",\"max_steps\":" << state.max_steps
        << ",\"finish_reason\":\"" << EscapeJson(state.finish_reason) << "\""
        << ",\"preview_version\":" << state.preview_version
        << ",\"preview_available\":" << (!state.latest_preview.empty() ? "true" : "false");
    if (!state.last_trace_json.empty()) {
        out << ",\"last_trace\":" << state.last_trace_json;
    }
    out << "}";
    return out.str();
}

void RunAsyncAt(camcap::Camera &camera,
                at::AtOrchestrator &orchestrator,
                camcap::CameraParams &current_params,
                AsyncRunState &state,
                const int max_steps,
                const int preview_every)
{
    for (int index = 0; index < max_steps && !state.stop_requested.load(); ++index) {
        const auto started = std::chrono::steady_clock::now();
        if (auto set = camera.setParams(current_params); !set) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.running = false;
            state.finished = true;
            state.finish_reason = "CameraSetParamsFailed: " + set.error().message;
            return;
        }
        const auto set_end = std::chrono::steady_clock::now();
        auto mat = CaptureMat(camera);
        if (!mat) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.running = false;
            state.finished = true;
            state.finish_reason = "CameraCaptureFailed: " + mat.error().message;
            return;
        }
        const auto capture_end = std::chrono::steady_clock::now();
        at::FrameContext input{mat.value(), ToAtParams(current_params)};
        const at::StepResult result = orchestrator.ProcessStep(input);
        const auto at_end = std::chrono::steady_clock::now();

        StepTiming timing;
        timing.set_params_ms = std::chrono::duration<double, std::milli>(set_end - started).count();
        timing.capture_ms = std::chrono::duration<double, std::milli>(capture_end - set_end).count();
        timing.at_core_ms = std::chrono::duration<double, std::milli>(at_end - capture_end).count();
        timing.total_ms = std::chrono::duration<double, std::milli>(at_end - started).count();
        const std::string trace = TraceJson(result, input.current_params, "", &orchestrator, &timing);

        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.completed_steps = index + 1;
            state.last_trace_json = trace;
            // No encoding, no disk IO and no network IO in the AT critical path.
            if (preview_every > 0 && ((index + 1) % preview_every == 0 || result.finished)) {
                state.latest_preview = mat.value().clone();
                ++state.preview_version;
            }
        }
        current_params = ToCamcapParams(result.next_params, current_params);
        if (result.finished) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.running = false;
            state.finished = true;
            state.finish_reason = at::ToString(result.trace.finish_reason);
            return;
        }
    }

    std::lock_guard<std::mutex> lock(state.mutex);
    state.running = false;
    state.finished = true;
    state.finish_reason = state.stop_requested.load() ? "Stopped" : "MaxStepsReached";
}

int RunServer(const Options &options)
{
    at::AtOrchestrator orchestrator(MakeSessionConfig(options));

    camcap::DeviceConfig device_config;
    device_config.device = options.device;
    camcap::Camera camera(device_config);
    std::cerr << "camera open start: device=" << options.device << '\n';
    if (auto opened = camera.open(); !opened) {
        std::cerr << "open camera failed: " << opened.error().message << '\n';
        return 1;
    }
    std::cerr << "camera open done\n";

    camcap::CameraParams current_params = options.initial_params;
    AsyncRunState async_run;
    if (auto set = camera.setParams(current_params); !set) {
        std::cerr << "set initial params failed: " << set.error().message << '\n';
        return 1;
    }

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    constexpr int opt = 1;
    (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(options.port));
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, 4) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }

    std::cout << "at_device_runner listening on port " << options.port << '\n';
    bool running = true;
    while (running) {
        const int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        while (running) {
            auto command_text = ReceiveCommand(client_fd);
            if (!command_text) {
                break;
            }

            const auto command = FindString(*command_text, "command");
            if (!command) {
                (void)SendResponse(client_fd,
                                   ErrorResponse(camera, camcap::makeError(
                                                             camcap::ErrorCode::ProtocolError,
                                                             "missing command field")));
                continue;
            }

            if (*command == "get_status") {
                (void)SendResponse(client_fd, OkResponse(camera));
            } else if (*command == "get_run_status") {
                Response response = OkResponse(camera);
                response.at_json = AsyncRunJson(async_run);
                (void)SendResponse(client_fd, response);
            } else if (*command == "run_at_async") {
                const int max_steps = FindInt(*command_text, "max_steps").value_or(80);
                const int preview_every = FindInt(*command_text, "preview_every").value_or(0);
                if (max_steps <= 0 || max_steps > 10000 || preview_every < 0 || preview_every > 10000) {
                    (void)SendResponse(client_fd, ErrorResponse(camera, camcap::makeError(
                        camcap::ErrorCode::ProtocolError, "max_steps must be 1..10000 and preview_every must be 0..10000")));
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lock(async_run.mutex);
                    if (async_run.running) {
                        (void)SendResponse(client_fd, ErrorResponse(camera, camcap::makeError(
                            camcap::ErrorCode::ProtocolError, "an async AT run is already active")));
                        continue;
                    }
                }
                if (async_run.worker.joinable()) {
                    async_run.worker.join();
                }
                if (auto params = ParseParams(*command_text)) {
                    current_params = *params;
                }
                orchestrator.Reset();
                {
                    std::lock_guard<std::mutex> lock(async_run.mutex);
                    async_run.stop_requested.store(false);
                    async_run.running = true;
                    async_run.finished = false;
                    async_run.max_steps = max_steps;
                    async_run.completed_steps = 0;
                    async_run.finish_reason = "Running";
                    async_run.last_trace_json.clear();
                    async_run.latest_preview.release();
                    async_run.preview_version = 0;
                }
                async_run.worker = std::thread(RunAsyncAt, std::ref(camera), std::ref(orchestrator),
                                                std::ref(current_params), std::ref(async_run), max_steps, preview_every);
                Response response = OkResponse(camera);
                response.at_json = AsyncRunJson(async_run);
                (void)SendResponse(client_fd, response);
            } else if (*command == "stop_at_async") {
                async_run.stop_requested.store(true);
                Response response = OkResponse(camera);
                response.at_json = AsyncRunJson(async_run);
                (void)SendResponse(client_fd, response);
            } else if (*command == "get_preview") {
                cv::Mat preview;
                {
                    std::lock_guard<std::mutex> lock(async_run.mutex);
                    preview = async_run.latest_preview.clone();
                }
                Response response = OkResponse(camera);
                response.at_json = AsyncRunJson(async_run);
                if (!preview.empty()) {
                    response.image_encoding = "png";
                    if (!EncodePng(preview, response.image)) {
                        response = ErrorResponse(camera, camcap::makeError(camcap::ErrorCode::EncodeFailed,
                                                                             "failed to encode deferred preview"));
                    }
                }
                (void)SendResponse(client_fd, response);
            } else if ([&async_run] {
                           std::lock_guard<std::mutex> lock(async_run.mutex);
                           return async_run.running;
                       }()) {
                (void)SendResponse(client_fd, ErrorResponse(camera, camcap::makeError(
                    camcap::ErrorCode::ProtocolError,
                    "camera commands are disabled during async AT; use get_run_status, get_preview or stop_at_async")));
            } else if (*command == "set_params") {
                auto params = ParseParams(*command_text);
                if (!params) {
                    (void)SendResponse(client_fd,
                                       ErrorResponse(camera, camcap::makeError(
                                                                 camcap::ErrorCode::ProtocolError,
                                                                 "set_params requires exposure_us, gain, focus and lights[4]")));
                    continue;
                }
                current_params = *params;
                auto set = camera.setParams(current_params);
                (void)SendResponse(client_fd, set ? OkResponse(camera) : ErrorResponse(camera, set.error()));
            } else if (*command == "capture") {
                if (auto encoding = FindString(*command_text, "encoding"); encoding && *encoding != "png") {
                    (void)SendResponse(client_fd,
                                       ErrorResponse(camera, camcap::makeError(
                                                                 camcap::ErrorCode::ProtocolError,
                                                                 "only png encoding is supported")));
                    continue;
                }
                (void)SendResponse(client_fd, CaptureResponse(camera));
            } else if (*command == "capture_heatmap") {
                if (auto encoding = FindString(*command_text, "encoding"); encoding && *encoding != "png") {
                    (void)SendResponse(client_fd,
                                       ErrorResponse(camera, camcap::makeError(
                                                                 camcap::ErrorCode::ProtocolError,
                                                                 "only png encoding is supported")));
                    continue;
                }
                const bool overlay = FindBool(*command_text, "overlay").value_or(true);
                (void)SendResponse(client_fd,
                                   CaptureHeatmapResponse(camera, orchestrator, current_params, overlay));
            } else if (*command == "close_lights") {
                current_params.lights = {0, 0, 0, 0};
                auto set = camera.setParams(current_params);
                (void)SendResponse(client_fd, set ? OkResponse(camera) : ErrorResponse(camera, set.error()));
            } else if (*command == "reset_at") {
                orchestrator.Reset();
                if (auto params = ParseParams(*command_text)) {
                    current_params = *params;
                    if (auto set = camera.setParams(current_params); !set) {
                        (void)SendResponse(client_fd, ErrorResponse(camera, set.error()));
                        continue;
                    }
                }
                Response response = OkResponse(camera);
                response.at_json = "{\"finished\":false,\"need_decode\":false,\"step\":0,\"reset\":true}";
                (void)SendResponse(client_fd, response);
            } else if (*command == "at_step") {
                if (auto encoding = FindString(*command_text, "encoding"); encoding && *encoding != "png") {
                    (void)SendResponse(client_fd,
                                       ErrorResponse(camera, camcap::makeError(
                                                                 camcap::ErrorCode::ProtocolError,
                                                                 "only png encoding is supported")));
                    continue;
                }
                (void)SendResponse(client_fd, AtStepResponse(camera, orchestrator, current_params));
            } else if (*command == "shutdown") {
                (void)SendResponse(client_fd, OkResponse(camera));
                running = false;
            } else {
                (void)SendResponse(client_fd,
                                   ErrorResponse(camera, camcap::makeError(
                                                             camcap::ErrorCode::ProtocolError,
                                                             "unknown command: " + *command)));
            }
        }

        close(client_fd);
    }

    async_run.stop_requested.store(true);
    if (async_run.worker.joinable()) {
        async_run.worker.join();
    }
    close(server_fd);
    return 0;
}

}  // namespace

int main(int argc, char **argv)
{
    Options options;
    try {
        options = ParseArgs(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        PrintUsage(argv[0]);
        return 2;
    }

    try {
        return options.server ? RunServer(options) : RunBatch(options);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
