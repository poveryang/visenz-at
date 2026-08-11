#include "at_run_log.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace at {
namespace {

std::int64_t WallTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace

AtRunLog::AtRunLog(RunLogConfig config) : config_(std::move(config))
{
    active_ = config_.enable;
}

std::string AtRunLog::ResolveDir(const RunLogConfig &config)
{
    if (!config.dir.empty()) {
        return config.dir;
    }
    if (const char *env = std::getenv("AT_LOG_DIR")) {
        if (env[0] != '\0') {
            return std::string(env);
        }
    }
    // 设备上 /usr/scanner 是独立大分区；没有则回落到 /tmp
    if (std::filesystem::exists("/usr/scanner/Log") ||
        std::filesystem::exists("/usr/scanner")) {
        return "/usr/scanner/Log/at";
    }
    return "/tmp/visenz_at";
}

std::string AtRunLog::MakeRunId()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    static unsigned counter = 0;
    ++counter;
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms
        << '_' << std::setw(4) << (counter % 10000);
    return out.str();
}

std::string AtRunLog::Escape(const std::string &text)
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

std::string AtRunLog::RectJson(const cv::Rect &rect)
{
    std::ostringstream out;
    out << "{\"x\":" << rect.x << ",\"y\":" << rect.y << ",\"width\":" << rect.width
        << ",\"height\":" << rect.height << "}";
    return out.str();
}

std::string AtRunLog::ParamsJson(const CameraParams &params)
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

std::string AtRunLog::QualityJson(const ImageQuality &quality)
{
    std::ostringstream out;
    out << "{\"brightness\":" << quality.brightness << ",\"contrast\":" << quality.contrast
        << ",\"sharpness\":" << quality.sharpness
        << ",\"saturation_ratio\":" << quality.saturation_ratio
        << ",\"noise_proxy\":" << quality.noise_proxy << "}";
    return out.str();
}

std::string AtRunLog::HeatmapJson(const HeatmapObservation &heatmap)
{
    std::ostringstream out;
    out << "{\"available\":" << (heatmap.available ? "true" : "false")
        << ",\"roi\":" << RectJson(heatmap.roi) << ",\"confidence\":" << heatmap.confidence
        << ",\"feature_strength\":" << heatmap.feature_strength
        << ",\"stability\":" << heatmap.stability << ",\"source\":\"" << Escape(heatmap.source)
        << "\",\"model_version\":\"" << Escape(heatmap.model_version) << "\"}";
    return out.str();
}

void AtRunLog::CloseFile()
{
    if (out_.is_open()) {
        out_.flush();
        out_.close();
    }
    begun_ = false;
}

void AtRunLog::WriteLine(const std::string &json)
{
    if (!out_.is_open()) {
        return;
    }
    out_ << json << '\n';
    out_.flush();
}

void AtRunLog::WriteLatestPointers() const
{
    if (dir_.empty()) {
        return;
    }
    {
        std::ofstream latest(std::filesystem::path(dir_) / "latest.path",
                             std::ios::out | std::ios::trunc);
        if (latest.is_open()) {
            latest << path_;
        }
    }
    WriteInitStatus(config_,
                    version_,
                    heatmap_model_,
                    begun_ ? "running" : "ended",
                    {},
                    path_);
}

void AtRunLog::WriteInitStatus(const RunLogConfig &config,
                               const std::string &version,
                               const std::string &heatmap_model,
                               const std::string &status,
                               const std::string &detail,
                               const std::string &run_log_path)
{
    const std::filesystem::path dir = ResolveDir(config);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return;
    }
    std::ofstream summary(dir / "latest_init.txt", std::ios::out | std::ios::trunc);
    if (!summary.is_open()) {
        return;
    }
    summary << "status=" << status << '\n'
            << "version=" << version << '\n'
            << "heatmap_model=" << (heatmap_model.empty() ? "NullHeatmap" : heatmap_model)
            << '\n'
            << "run_log=" << run_log_path << '\n'
            << "log_dir=" << dir.string() << '\n';
    if (!detail.empty()) {
        summary << "detail=" << detail << '\n';
    }
}

void AtRunLog::PruneOldRuns() const
{
    if (dir_.empty()) {
        return;
    }
    std::vector<std::filesystem::directory_entry> runs;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind("run_", 0) == 0 && entry.path().extension() == ".jsonl") {
            runs.push_back(entry);
        }
    }
    if (static_cast<int>(runs.size()) <= kDefaultKeepRuns) {
        return;
    }
    std::sort(runs.begin(), runs.end(), [](const auto &a, const auto &b) {
        return a.last_write_time() > b.last_write_time();
    });
    for (std::size_t i = static_cast<std::size_t>(kDefaultKeepRuns); i < runs.size(); ++i) {
        std::filesystem::remove(runs[i].path(), ec);
    }
}

void AtRunLog::Begin(const SessionConfig &session)
{
    if (!active_) {
        return;
    }
    End(nullptr);

    dir_ = ResolveDir(config_);
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        active_ = false;
        return;
    }

    version_ =
#if defined(AT_VERSION_STR)
        AT_VERSION_STR
#else
        "0.0.0-dev"
#endif
        ;
    heatmap_model_ = session.heatmap.model_path;

    run_id_ = MakeRunId();
    path_ = (std::filesystem::path(dir_) / ("run_" + run_id_ + ".jsonl")).string();
    out_.open(path_, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) {
        active_ = false;
        path_.clear();
        run_id_.clear();
        WriteInitStatus(config_, version_, heatmap_model_, "error", "open run log failed");
        return;
    }

    begun_ = true;
    steps_ = 0;
    start_ = std::chrono::steady_clock::now();
    PruneOldRuns();
    WriteLatestPointers();

    std::ostringstream line;
    line << "{\"event\":\"begin\",\"schema\":" << kSchemaVersion << ",\"run_id\":\""
         << Escape(run_id_) << "\",\"ts_ms\":" << WallTimeMs() << ",\"version\":\""
         << Escape(version_) << "\",\"heatmap_model\":\"" << Escape(heatmap_model_) << "\""
         << ",\"heatmap_context\":\"" << Escape(session.heatmap.context) << "\""
         << ",\"heatmap_precision\":\"" << Escape(session.heatmap.precision) << "\""
         << ",\"flow\":{\"enable_heatmap\":" << (session.flow.enable_heatmap ? "true" : "false")
         << ",\"enable_decode_ranking\":"
         << (session.flow.enable_decode_ranking ? "true" : "false")
         << ",\"focus_tune_steps\":" << session.flow.focus_tune_steps
         << ",\"exposure_steps_per_profile\":" << session.flow.exposure_steps_per_profile << "}"
         << ",\"budget\":{\"max_steps\":" << session.budget.max_steps
         << ",\"decode_budget\":" << session.budget.decode_budget
         << ",\"focus_step\":" << session.budget.focus_step
         << ",\"exposure_step\":" << session.budget.exposure_step << "}"
         << ",\"camera\":{\"min_focus\":" << session.camera.min_focus_pos
         << ",\"max_focus\":" << session.camera.max_focus_pos
         << ",\"min_et\":" << session.camera.min_exp_time
         << ",\"max_et\":" << session.camera.max_exp_time << "}}";
    WriteLine(line.str());
}

void AtRunLog::AppendStep(const CameraParams &params_before, const StepResult &result)
{
    if (!active_ || !begun_) {
        return;
    }
    ++steps_;
    std::ostringstream line;
    line << "{\"event\":\"step\",\"schema\":" << kSchemaVersion << ",\"run_id\":\""
         << Escape(run_id_) << "\",\"ts_ms\":" << WallTimeMs()
         << ",\"step\":" << result.trace.step_index
         << ",\"stage_step_count\":" << result.trace.stage_step_count << ",\"phase\":\""
         << Escape(ToString(result.trace.phase)) << "\",\"action\":\""
         << Escape(ToString(result.trace.action)) << "\",\"finished\":"
         << (result.finished ? "true" : "false") << ",\"need_decode\":"
         << (result.need_decode ? "true" : "false") << ",\"finish_reason\":\""
         << Escape(ToString(result.trace.finish_reason)) << "\",\"decode_used\":"
         << result.trace.decode_used << ",\"params\":" << ParamsJson(params_before)
         << ",\"next_params\":" << ParamsJson(result.next_params)
         << ",\"quality\":" << QualityJson(result.trace.quality)
         << ",\"heatmap\":" << HeatmapJson(result.trace.heatmap) << ",\"reason\":\""
         << Escape(result.trace.reason) << "\"}";
    WriteLine(line.str());
}

void AtRunLog::End(const StepResult *final_result)
{
    if (!active_ || !begun_) {
        return;
    }

    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start_)
                             .count();
    std::ostringstream line;
    line << "{\"event\":\"end\",\"schema\":" << kSchemaVersion << ",\"run_id\":\""
         << Escape(run_id_) << "\",\"ts_ms\":" << WallTimeMs() << ",\"steps\":" << steps_
         << ",\"elapsed_ms\":" << elapsed;
    if (final_result != nullptr) {
        line << ",\"finished\":" << (final_result->finished ? "true" : "false")
             << ",\"finish_reason\":\"" << Escape(ToString(final_result->trace.finish_reason))
             << "\"";
        if (final_result->best_candidate.has_value()) {
            line << ",\"best_params\":" << ParamsJson(final_result->best_candidate->params)
                 << ",\"best_score\":" << final_result->best_candidate->score;
        } else {
            line << ",\"best_params\":" << ParamsJson(final_result->next_params);
        }
    } else {
        line << ",\"finished\":false,\"finish_reason\":\"Interrupted\"";
    }
    line << "}";
    WriteLine(line.str());
    CloseFile();
    WriteInitStatus(config_, version_, heatmap_model_, "ended", {}, path_);
}

}  // namespace at
