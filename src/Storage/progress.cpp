#include "Storage/progress.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace
{
void AppendEscapedJsonString(std::string * out, std::string_view value)
{
    out->push_back('"');
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
            case '\\':
                out->append("\\\\");
                break;
            case '"':
                out->append("\\\"");
                break;
            case '\n':
                out->append("\\n");
                break;
            case '\r':
                out->append("\\r");
                break;
            case '\t':
                out->append("\\t");
                break;
            default:
                if (ch < 0x20)
                {
                    std::ostringstream escaped;
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
                    out->append(escaped.str());
                }
                else
                {
                    out->push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    out->push_back('"');
}

std::string BuildRunStartedLine(const RunStartedEvent & event)
{
    std::ostringstream out;
    out << "{\"event\":\"run_started\",\"selected_formats\":[";
    for (std::size_t i = 0; i < event.selected_formats.size(); ++i)
    {
        if (i > 0)
        {
            out << ',';
        }
        std::string escaped;
        AppendEscapedJsonString(&escaped, event.selected_formats[i]);
        out << escaped;
    }
    out << "],\"scale_factor\":" << std::setprecision(17) << event.scale_factor << ",\"output_path\":";
    std::string escaped;
    AppendEscapedJsonString(&escaped, event.output_path);
    out << escaped << '}';
    return out.str();
}

std::string BuildFormatStartedLine(const FormatStartedEvent & event)
{
    std::string escaped_format;
    AppendEscapedJsonString(&escaped_format, event.format);

    std::ostringstream out;
    out << "{\"event\":\"format_started\",\"format\":" << escaped_format << ",\"index\":" << event.index
        << ",\"total_formats\":" << event.total_formats << '}';
    return out.str();
}

std::string BuildTableStartedLine(const TableStartedEvent & event)
{
    std::string escaped_format;
    std::string escaped_table;
    AppendEscapedJsonString(&escaped_format, event.format);
    AppendEscapedJsonString(&escaped_table, event.table);

    std::ostringstream out;
    out << "{\"event\":\"table_started\",\"format\":" << escaped_format << ",\"table\":" << escaped_table
        << ",\"total_rows\":" << event.total_rows << ",\"part_count\":" << event.part_count << '}';
    return out.str();
}

std::string BuildTableProgressLine(const TableProgressEvent & event)
{
    std::string escaped_format;
    std::string escaped_table;
    AppendEscapedJsonString(&escaped_format, event.format);
    AppendEscapedJsonString(&escaped_table, event.table);

    std::ostringstream out;
    out << "{\"event\":\"table_progress\",\"format\":" << escaped_format << ",\"table\":" << escaped_table
        << ",\"rows_written\":" << event.rows_written << ",\"total_rows\":" << event.total_rows
        << ",\"completed_parts\":" << event.completed_parts << '}';
    return out.str();
}

std::string BuildTableFinishedLine(const TableFinishedEvent & event)
{
    std::string escaped_format;
    std::string escaped_table;
    AppendEscapedJsonString(&escaped_format, event.format);
    AppendEscapedJsonString(&escaped_table, event.table);

    std::ostringstream out;
    out << "{\"event\":\"table_finished\",\"format\":" << escaped_format << ",\"table\":" << escaped_table
        << ",\"rows_written\":" << event.rows_written << ",\"total_rows\":" << event.total_rows << '}';
    return out.str();
}

std::string BuildTableFailedLine(const TableFailedEvent & event)
{
    std::string escaped_format;
    std::string escaped_table;
    std::string escaped_error;
    AppendEscapedJsonString(&escaped_format, event.format);
    AppendEscapedJsonString(&escaped_table, event.table);
    AppendEscapedJsonString(&escaped_error, event.error);

    std::ostringstream out;
    out << "{\"event\":\"table_failed\",\"format\":" << escaped_format << ",\"table\":" << escaped_table
        << ",\"error\":" << escaped_error << '}';
    return out.str();
}

std::string BuildFormatFinishedLine(const FormatFinishedEvent & event)
{
    std::string escaped_format;
    AppendEscapedJsonString(&escaped_format, event.format);

    std::ostringstream out;
    out << "{\"event\":\"format_finished\",\"format\":" << escaped_format << ",\"success\":"
        << (event.success ? "true" : "false") << '}';
    return out.str();
}

std::string BuildRunFinishedLine(const RunFinishedEvent & event)
{
    std::ostringstream out;
    out << "{\"event\":\"run_finished\",\"success\":" << (event.success ? "true" : "false") << '}';
    return out.str();
}
} // namespace

JsonProgressSink::JsonProgressSink(std::ostream & out)
    : out_(out)
{
}

void JsonProgressSink::RunStarted(const RunStartedEvent & event)
{
    WriteLine(BuildRunStartedLine(event));
}

void JsonProgressSink::FormatStarted(const FormatStartedEvent & event)
{
    WriteLine(BuildFormatStartedLine(event));
}

void JsonProgressSink::TableStarted(const TableStartedEvent & event)
{
    WriteLine(BuildTableStartedLine(event));
}

void JsonProgressSink::TableProgress(const TableProgressEvent & event)
{
    WriteLine(BuildTableProgressLine(event));
}

void JsonProgressSink::TableFinished(const TableFinishedEvent & event)
{
    WriteLine(BuildTableFinishedLine(event));
}

void JsonProgressSink::TableFailed(const TableFailedEvent & event)
{
    WriteLine(BuildTableFailedLine(event));
}

void JsonProgressSink::FormatFinished(const FormatFinishedEvent & event)
{
    WriteLine(BuildFormatFinishedLine(event));
}

void JsonProgressSink::RunFinished(const RunFinishedEvent & event)
{
    WriteLine(BuildRunFinishedLine(event));
}

void JsonProgressSink::WriteLine(const std::string & line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << line << '\n';
    out_.flush();
}
