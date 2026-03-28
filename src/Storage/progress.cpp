#include "Storage/progress.h"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

namespace po = boost::program_options;

namespace
{
std::string ToLower(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string StatusGlyph(TableProgressStatus status)
{
    switch (status)
    {
        case TableProgressStatus::Pending:
            return " ";
        case TableProgressStatus::Running:
            return " ";
        case TableProgressStatus::Done:
            return "\xE2\x9C\x93";
        case TableProgressStatus::Failed:
            return "!";
    }
    return "?";
}

std::uint64_t ApproxPercent(const TableProgressState & state)
{
    if (state.status == TableProgressStatus::Done)
    {
        return 100;
    }

    if (state.status == TableProgressStatus::Pending)
    {
        return 0;
    }

    if (state.part_count <= 0)
    {
        if (state.total_rows == 0 || state.rows_written == 0)
        {
            return 0;
        }

        const auto bounded_rows = std::min(state.rows_written, state.total_rows);
        const auto percent = (bounded_rows * 99) / std::max<std::uint64_t>(1, state.total_rows);
        return std::min<std::uint64_t>(99, std::max<std::uint64_t>(25, percent));
    }

    const auto part_count = static_cast<std::uint64_t>(std::max<std::int32_t>(1, state.part_count));
    const auto completed_parts =
        static_cast<std::uint64_t>(std::clamp(state.completed_parts, 0, state.part_count));

    double approx_completed_parts = static_cast<double>(completed_parts);
    if (completed_parts < part_count && state.rows_written > 0)
    {
        if (state.total_rows > 0)
        {
            const auto estimated_rows_per_part =
                std::max(1.0, static_cast<double>(state.total_rows) / static_cast<double>(part_count));
            const auto estimated_completed_rows = std::min(
                static_cast<double>(state.rows_written),
                estimated_rows_per_part * static_cast<double>(completed_parts));
            const auto active_rows = std::max(0.0, static_cast<double>(state.rows_written) - estimated_completed_rows);
            if (active_rows > 0.0)
            {
                approx_completed_parts += std::clamp(active_rows / estimated_rows_per_part, 0.25, 0.99);
            }
            else if (completed_parts == 0)
            {
                approx_completed_parts += 0.25;
            }
        }
        else if (completed_parts == 0)
        {
            approx_completed_parts += 0.25;
        }
    }

    const auto percent = static_cast<std::uint64_t>((approx_completed_parts * 100.0) / static_cast<double>(part_count));
    return std::min<std::uint64_t>(99, percent);
}
} // namespace

struct CliProgressController::TableEntry
{
    TableProgressState state;
    Clock::time_point last_emit_at{};
    std::uint64_t last_emitted_percent = 0;
    bool has_emitted = false;
};

void RegisterProgressCliOptions(po::options_description & desc)
{
    desc.add_options()(
        "verbosity",
        po::value<std::string>()->default_value(std::string(ToString(CliVerbosity::Quiet))),
        "CLI progress verbosity: quiet | verbose");
}

arrow::Result<CliVerbosity> BuildCliVerbosity(const po::variables_map & vm)
{
    const auto raw = ToLower(vm["verbosity"].as<std::string>());
    if (raw == "quiet")
    {
        return CliVerbosity::Quiet;
    }
    if (raw == "verbose")
    {
        return CliVerbosity::Verbose;
    }
    return arrow::Status::Invalid("Unsupported verbosity: ", raw);
}

std::string_view ToString(CliVerbosity verbosity)
{
    switch (verbosity)
    {
        case CliVerbosity::Quiet:
            return "quiet";
        case CliVerbosity::Verbose:
            return "verbose";
    }
    return "quiet";
}

std::string FormatTableProgressLine(const TableProgressState & state, std::size_t bar_width)
{
    const auto percent = ApproxPercent(state);
    const auto filled = bar_width == 0 ? 0 : static_cast<std::size_t>((percent * bar_width) / 100);
    const auto progress_label =
        state.status == TableProgressStatus::Done ? std::to_string(percent) + "%" : "~" + std::to_string(percent) + "%";

    std::ostringstream out;
    out << '[' << StatusGlyph(state.status) << "] " << state.table_name << ' ';
    out << std::setw(4) << progress_label << " [";
    out << std::string(filled, '#');
    out << std::string(bar_width - filled, '-');
    out << "] " << state.rows_written << " rows";
    if (state.part_count > 0)
    {
        out << " parts " << state.completed_parts << '/' << state.part_count;
    }
    return out.str();
}

CliProgressController::CliProgressController(
    std::vector<CliProgressTable> tables,
    CliProgressControllerOptions options)
    : options_(std::move(options))
{
    if (options_.stream == nullptr)
    {
        options_.stream = &std::cerr;
    }
    if (!options_.now)
    {
        options_.now = []() { return Clock::now(); };
    }
    if (options_.tty_refresh_interval.count() <= 0)
    {
        options_.tty_refresh_interval = std::chrono::milliseconds(100);
    }
    if (options_.non_tty_update_interval.count() <= 0)
    {
        options_.non_tty_update_interval = std::chrono::milliseconds(1000);
    }

    stream_ = options_.stream;
    interactive_tty_ = options_.verbosity == CliVerbosity::Verbose && options_.is_tty;
    append_only_ = options_.verbosity == CliVerbosity::Verbose && !options_.is_tty;

    tables_.reserve(tables.size());
    for (std::size_t index = 0; index < tables.size(); ++index)
    {
        TableEntry entry;
        entry.state.index = index;
        entry.state.table_name = std::move(tables[index].table_name);
        entry.state.total_rows = tables[index].total_rows;
        tables_.push_back(std::move(entry));
    }

    if (interactive_tty_ && !tables_.empty())
    {
        {
            std::lock_guard<yaclib_std::mutex> lock(mutex_);
            render_dirty_ = true;
            RenderFrameLocked();
        }
        render_thread_ = yaclib_std::thread(&CliProgressController::RenderLoop, this);
    }
}

CliProgressController::~CliProgressController()
{
    Stop();
}

auto CliProgressController::LookupEntry(std::size_t table_index) -> TableEntry *
{
    if (table_index >= tables_.size())
    {
        return nullptr;
    }
    return &tables_[table_index];
}

auto CliProgressController::LookupEntry(std::size_t table_index) const -> const TableEntry *
{
    if (table_index >= tables_.size())
    {
        return nullptr;
    }
    return &tables_[table_index];
}

void CliProgressController::NotifyObserver(const TableProgressState & state) const
{
    if (options_.state_observer)
    {
        options_.state_observer(state);
    }
}

void CliProgressController::RequestRender(bool immediate)
{
    if (!interactive_tty_)
    {
        return;
    }

    if (immediate)
    {
        RenderFrameLocked();
        return;
    }

    render_dirty_ = true;
    render_cv_.notify_all();
}

void CliProgressController::MaybeEmitAppendOnly(TableEntry & entry, bool force)
{
    if (!append_only_)
    {
        return;
    }

    const auto now = options_.now();
    const auto percent = ApproxPercent(entry.state);
    const bool interval_elapsed =
        !entry.has_emitted || now - entry.last_emit_at >= options_.non_tty_update_interval;
    const bool percent_step_reached =
        !entry.has_emitted || percent >= entry.last_emitted_percent + 5 || percent == 100;

    if (!force && !interval_elapsed && !percent_step_reached)
    {
        return;
    }

    (*stream_) << FormatTableProgressLine(entry.state) << '\n';
    stream_->flush();
    entry.last_emit_at = now;
    entry.last_emitted_percent = percent;
    entry.has_emitted = true;
}

void CliProgressController::WriteFailureMessage(const std::string & message)
{
    if (!interactive_tty_)
    {
        (*stream_) << message << '\n';
        stream_->flush();
        return;
    }

    ClearFrameLocked(/*return_to_frame_start=*/true);
    (*stream_) << "\r\x1b[2K" << message << '\n';
    frame_drawn_ = false;
    rendered_lines_ = 0;
    render_dirty_ = true;
    RenderFrameLocked();
}

void CliProgressController::RenderFrameLocked()
{
    if (!interactive_tty_ || tables_.empty())
    {
        render_dirty_ = false;
        return;
    }

    if (frame_drawn_ && rendered_lines_ > 0)
    {
        (*stream_) << "\x1b[" << rendered_lines_ << 'A';
    }

    for (const auto & entry : tables_)
    {
        (*stream_) << "\r\x1b[2K" << FormatTableProgressLine(entry.state) << '\n';
    }
    stream_->flush();

    rendered_lines_ = tables_.size();
    frame_drawn_ = true;
    render_dirty_ = false;
}

void CliProgressController::ClearFrameLocked(bool return_to_frame_start)
{
    if (!interactive_tty_ || !frame_drawn_ || rendered_lines_ == 0)
    {
        return;
    }

    (*stream_) << "\x1b[" << rendered_lines_ << 'A';
    for (std::size_t i = 0; i < rendered_lines_; ++i)
    {
        (*stream_) << "\r\x1b[2K\n";
    }
    if (return_to_frame_start)
    {
        (*stream_) << "\x1b[" << rendered_lines_ << 'A';
    }
    stream_->flush();
}

void CliProgressController::RenderLoop()
{
    std::unique_lock<yaclib_std::mutex> lock(mutex_);
    while (!stop_requested_)
    {
        render_cv_.wait(lock, [this]() { return stop_requested_ || render_dirty_; });
        if (stop_requested_)
        {
            break;
        }
        if (render_dirty_)
        {
            RenderFrameLocked();
        }
    }
}

void CliProgressController::MarkTableStarted(std::size_t table_index, std::int32_t part_count)
{
    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    auto * entry = LookupEntry(table_index);
    if (entry == nullptr || entry->state.status == TableProgressStatus::Failed)
    {
        return;
    }

    entry->state.part_count = std::max<std::int32_t>(0, part_count);
    entry->state.status = TableProgressStatus::Running;
    MaybeEmitAppendOnly(*entry, /*force=*/true);
    RequestRender(/*immediate=*/false);
    NotifyObserver(entry->state);
}

void CliProgressController::AddCommittedRows(std::size_t table_index, std::uint64_t rows)
{
    if (rows == 0)
    {
        return;
    }

    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    auto * entry = LookupEntry(table_index);
    if (entry == nullptr || entry->state.status == TableProgressStatus::Failed || entry->state.status == TableProgressStatus::Done)
    {
        return;
    }

    entry->state.status = TableProgressStatus::Running;
    if (std::numeric_limits<std::uint64_t>::max() - entry->state.rows_written < rows)
    {
        entry->state.rows_written = std::numeric_limits<std::uint64_t>::max();
    }
    else
    {
        entry->state.rows_written += rows;
    }
    MaybeEmitAppendOnly(*entry, /*force=*/false);
    RequestRender(/*immediate=*/false);
    NotifyObserver(entry->state);
}

void CliProgressController::MarkPartCompleted(std::size_t table_index)
{
    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    auto * entry = LookupEntry(table_index);
    if (entry == nullptr || entry->state.status == TableProgressStatus::Failed || entry->state.status == TableProgressStatus::Done)
    {
        return;
    }

    entry->state.status = TableProgressStatus::Running;
    ++entry->state.completed_parts;
    if (entry->state.part_count > 0)
    {
        entry->state.completed_parts = std::min(entry->state.completed_parts, entry->state.part_count);
    }
    MaybeEmitAppendOnly(*entry, /*force=*/false);
    RequestRender(/*immediate=*/false);
    NotifyObserver(entry->state);
}

void CliProgressController::MarkTableFinished(std::size_t table_index)
{
    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    auto * entry = LookupEntry(table_index);
    if (entry == nullptr || entry->state.status == TableProgressStatus::Failed)
    {
        return;
    }

    entry->state.status = TableProgressStatus::Done;
    if (entry->state.part_count > 0)
    {
        entry->state.completed_parts = entry->state.part_count;
    }
    MaybeEmitAppendOnly(*entry, /*force=*/true);
    RequestRender(/*immediate=*/true);
    NotifyObserver(entry->state);
}

void CliProgressController::ReportFailure(std::size_t table_index, std::string message)
{
    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    auto * entry = LookupEntry(table_index);
    if (entry != nullptr)
    {
        entry->state.status = TableProgressStatus::Failed;
        entry->state.last_error = message;
        MaybeEmitAppendOnly(*entry, /*force=*/true);
        NotifyObserver(entry->state);
    }

    WriteFailureMessage(message);
}

auto CliProgressController::Snapshot() const -> std::vector<TableProgressState>
{
    std::lock_guard<yaclib_std::mutex> lock(mutex_);
    std::vector<TableProgressState> snapshot;
    snapshot.reserve(tables_.size());
    for (const auto & entry : tables_)
    {
        snapshot.push_back(entry.state);
    }
    return snapshot;
}

void CliProgressController::Stop()
{
    {
        std::lock_guard<yaclib_std::mutex> lock(mutex_);
        if (stop_requested_)
        {
            return;
        }
        stop_requested_ = true;
        render_cv_.notify_all();
    }

    if (render_thread_.joinable())
    {
        render_thread_.join();
    }

    if (interactive_tty_)
    {
        std::lock_guard<yaclib_std::mutex> lock(mutex_);
        render_dirty_ = true;
        RenderFrameLocked();
    }
}
