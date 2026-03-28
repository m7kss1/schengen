#pragma once

#include <arrow/result.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace boost::program_options
{
class options_description;
class variables_map;
} // namespace boost::program_options

enum class CliVerbosity : std::uint8_t
{
    Quiet,
    Verbose,
};

void RegisterProgressCliOptions(boost::program_options::options_description & desc);
arrow::Result<CliVerbosity> BuildCliVerbosity(const boost::program_options::variables_map & vm);
std::string_view ToString(CliVerbosity verbosity);

enum class TableProgressStatus : std::uint8_t
{
    Pending,
    Running,
    Done,
    Failed,
};

struct CliProgressTable
{
    std::string table_name;
    std::uint64_t total_rows = 0;
};

struct TableProgressState
{
    std::size_t index = 0;
    std::string table_name;
    std::uint64_t total_rows = 0;
    std::uint64_t rows_written = 0;
    std::int32_t part_count = 0;
    std::int32_t completed_parts = 0;
    TableProgressStatus status = TableProgressStatus::Pending;
    std::string last_error;
};

struct CliProgressControllerOptions
{
    using Clock = std::chrono::steady_clock;

    CliVerbosity verbosity = CliVerbosity::Quiet;
    std::ostream * stream = nullptr;
    bool is_tty = false;
    std::chrono::milliseconds tty_refresh_interval{100};
    std::chrono::milliseconds non_tty_update_interval{1000};
    std::function<Clock::time_point()> now;
    std::function<void(const TableProgressState &)> state_observer;
};

std::string FormatTableProgressLine(
    const TableProgressState & state,
    std::size_t bar_width = 20,
    char running_spinner = '|');

class CliProgressController final
{
public:
    using Clock = CliProgressControllerOptions::Clock;

    explicit CliProgressController(
        std::vector<CliProgressTable> tables,
        CliProgressControllerOptions options = {});
    ~CliProgressController();

    CliProgressController(const CliProgressController &) = delete;
    auto operator=(const CliProgressController &) -> CliProgressController & = delete;

    void MarkTableStarted(std::size_t table_index, std::int32_t part_count);
    void AddCommittedRows(std::size_t table_index, std::uint64_t rows);
    void MarkPartCompleted(std::size_t table_index);
    void MarkTableFinished(std::size_t table_index);
    void ReportFailure(std::size_t table_index, std::string message);

    auto Snapshot() const -> std::vector<TableProgressState>;
    void Stop();

private:
    struct TableEntry;

    auto LookupEntry(std::size_t table_index) -> TableEntry *;
    auto LookupEntry(std::size_t table_index) const -> const TableEntry *;

    void NotifyObserver(const TableProgressState & state) const;
    void RequestRender(bool immediate);
    void MaybeEmitAppendOnly(TableEntry & entry, bool force);
    void WriteFailureMessage(const std::string & message);
    void RenderFrameLocked();
    void ClearFrameLocked(bool return_to_frame_start);
    void RenderLoop();

    mutable std::mutex mutex_;
    std::condition_variable render_cv_;
    std::vector<TableEntry> tables_;
    CliProgressControllerOptions options_;
    std::ostream * stream_ = nullptr;
    bool interactive_tty_ = false;
    bool append_only_ = false;
    bool render_dirty_ = false;
    bool stop_requested_ = false;
    bool frame_drawn_ = false;
    std::size_t rendered_lines_ = 0;
    std::size_t spinner_frame_ = 0;
    std::thread render_thread_;
};
