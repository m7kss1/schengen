#pragma once

#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct RunStartedEvent
{
    std::vector<std::string> selected_formats;
    double scale_factor = 1.0;
    std::string output_path;
};

struct FormatStartedEvent
{
    std::string format;
    std::int32_t index = 1;
    std::int32_t total_formats = 1;
};

struct TableStartedEvent
{
    std::string format;
    std::string table;
    std::uint64_t total_rows = 0;
    std::int32_t part_count = 1;
};

struct TableProgressEvent
{
    std::string format;
    std::string table;
    std::uint64_t rows_written = 0;
    std::uint64_t total_rows = 0;
    std::int32_t completed_parts = 0;
};

struct TableFinishedEvent
{
    std::string format;
    std::string table;
    std::uint64_t rows_written = 0;
    std::uint64_t total_rows = 0;
};

struct TableFailedEvent
{
    std::string format;
    std::string table;
    std::string error;
};

struct FormatFinishedEvent
{
    std::string format;
    bool success = false;
};

struct RunFinishedEvent
{
    bool success = false;
};

class IProgressSink
{
public:
    virtual ~IProgressSink() = default;

    virtual void RunStarted(const RunStartedEvent & event) = 0;
    virtual void FormatStarted(const FormatStartedEvent & event) = 0;
    virtual void TableStarted(const TableStartedEvent & event) = 0;
    virtual void TableProgress(const TableProgressEvent & event) = 0;
    virtual void TableFinished(const TableFinishedEvent & event) = 0;
    virtual void TableFailed(const TableFailedEvent & event) = 0;
    virtual void FormatFinished(const FormatFinishedEvent & event) = 0;
    virtual void RunFinished(const RunFinishedEvent & event) = 0;
};

class JsonProgressSink final : public IProgressSink
{
public:
    explicit JsonProgressSink(std::ostream & out);

    void RunStarted(const RunStartedEvent & event) override;
    void FormatStarted(const FormatStartedEvent & event) override;
    void TableStarted(const TableStartedEvent & event) override;
    void TableProgress(const TableProgressEvent & event) override;
    void TableFinished(const TableFinishedEvent & event) override;
    void TableFailed(const TableFailedEvent & event) override;
    void FormatFinished(const FormatFinishedEvent & event) override;
    void RunFinished(const RunFinishedEvent & event) override;

private:
    void WriteLine(const std::string & line);

    std::ostream & out_;
    std::mutex mutex_;
};
