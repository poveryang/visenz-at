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
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
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
    double heatmap_threshold{40.0};
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
              << " [--heatmap-model /path/model-uint8.tmfile]\n"
              << "       " << program
              << " --server [--device vs1000p_2mp] [--port 8080]"
              << " [--exposure-us 1000] [--gain 50] [--focus 30] [--lights 1,1,1,1]"
              << " [--heatmap-model /path/model-uint8.tmfile]\n"
              << "Heatmap options: [--heatmap-context timvx|cpu] [--heatmap-precision uint8|fp32]"
              << " [--heatmap-threshold 40] [--no-heatmap-overlay]\n";
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
                      const at::AtOrchestrator *orchestrator)
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
        if (auto set = camera.setParams(current); !set) {
            std::cerr << "set params failed: " << set.error().message << '\n';
            return 1;
        }

        auto mat = CaptureMat(camera);
        if (!mat) {
            std::cerr << "capture failed: " << mat.error().message << '\n';
            return 1;
        }

        at::FrameContext input;
        input.image = mat.value();
        input.current_params = ToAtParams(current);

        const at::StepResult result = orchestrator.ProcessStep(input);
        std::string image_path;
        if (!output_dir.empty()) {
            image_path = (output_dir / ("step_" + std::to_string(step) + ".png")).string();
            const cv::Mat display_image = orchestrator.BlendForDisplay(mat.value());
            if (!cv::imwrite(image_path, display_image)) {
                std::cerr << "failed to save image: " << image_path << '\n';
                return 1;
            }
        }

        const std::string trace_json = TraceJson(result, input.current_params, image_path, &orchestrator);
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
