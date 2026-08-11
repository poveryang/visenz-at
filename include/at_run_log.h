#ifndef AT_AT_RUN_LOG_H
#define AT_AT_RUN_LOG_H

#include "at_trace.h"
#include "at_types.h"

#include <chrono>
#include <fstream>
#include <string>

namespace at {

/**
 * 算法运行日志（与宿主软件 stdout/journal 隔离）。
 *
 * 一次 AT 运行 = 一个 JSONL 文件，事件仅三种：begin / step / end。
 *
 * 默认目录（可被 RunLogConfig::dir 或环境变量 AT_LOG_DIR 覆盖）：
 *   1) /usr/scanner/Log/at   （设备持久分区，优先）
 *   2) /tmp/visenz_at        （回落）
 *
 * 目录内约定：
 *   run_<id>.jsonl   — 完整逐步过程
 *   latest.path      — 最近一次 run 文件路径
 *   latest_init.txt  — 人类可读 Init 摘要
 */
class AtRunLog {
public:
    static constexpr int kSchemaVersion = 1;
    static constexpr int kDefaultKeepRuns = 30;

    explicit AtRunLog(RunLogConfig config = {});

    bool active() const { return active_; }
    const std::string &run_id() const { return run_id_; }
    const std::string &path() const { return path_; }
    const std::string &dir() const { return dir_; }

    void Begin(const SessionConfig &session);
    void AppendStep(const CameraParams &params_before, const StepResult &result);
    void End(const StepResult *final_result = nullptr);

    /// Init 失败等场景：不依赖已打开的 run 文件，仍写入 latest_init.txt
    static void WriteInitStatus(const RunLogConfig &config,
                                const std::string &version,
                                const std::string &heatmap_model,
                                const std::string &status,
                                const std::string &detail = {},
                                const std::string &run_log_path = {});

    static std::string ResolveDir(const RunLogConfig &config);

private:
    void WriteLine(const std::string &json);
    void CloseFile();
    void WriteLatestPointers() const;
    void PruneOldRuns() const;

    static std::string Escape(const std::string &text);
    static std::string ParamsJson(const CameraParams &params);
    static std::string QualityJson(const ImageQuality &quality);
    static std::string HeatmapJson(const HeatmapObservation &heatmap);
    static std::string RectJson(const cv::Rect &rect);
    static std::string MakeRunId();

    RunLogConfig config_{};
    bool active_ = false;
    bool begun_ = false;
    std::string dir_;
    std::string run_id_;
    std::string path_;
    std::ofstream out_;
    std::chrono::steady_clock::time_point start_{};
    int steps_ = 0;
    std::string version_;
    std::string heatmap_model_;
};

}  // namespace at

#endif  // AT_AT_RUN_LOG_H
