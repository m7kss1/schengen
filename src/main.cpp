#include "Common/registry.h"
#include "Storage/progress.h"
#include "Storage/storage.h"
#include "Storage/write_scheduler.h"
#include "Storage/writer.h"
#include "Tables/register_tables.h"
#include "yaclib/async/future.hpp"
#include "yaclib/async/run.hpp"
#include "yaclib/runtime/fair_thread_pool.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <unistd.h>
#include <vector>

namespace po = boost::program_options;

namespace
{
std::vector<std::string> DefaultTableNames()
{
    return {
        "region",
        "nation",
        "supplier",
        "part",
        "partsupp",
        "customer",
        "orders",
        "lineitem",
    };
}

void PrintTableList()
{
    for (const auto & name : DefaultTableNames())
    {
        std::cout << name << "\n";
    }
}

std::string JoinStrings(const std::vector<std::string> & values, std::string_view separator)
{
    if (values.empty())
    {
        return {};
    }

    std::string out;
    out.reserve(values.front().size() * values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            out += separator;
        }
        out += values[i];
    }
    return out;
}

std::string ToLower(std::string value)
{
    for (auto & ch : value)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string Trim(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\n\r");
    if (begin == std::string_view::npos)
    {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\n\r");
    return std::string(value.substr(begin, end - begin + 1));
}

std::vector<std::string> SplitFormatSpec(std::string_view format_spec)
{
    std::vector<std::string> formats;
    std::size_t begin = 0;
    while (begin <= format_spec.size())
    {
        const auto end = format_spec.find(',', begin);
        const auto token = end == std::string_view::npos ? format_spec.substr(begin) : format_spec.substr(begin, end - begin);
        auto format = ToLower(Trim(token));
        if (!format.empty())
        {
            formats.emplace_back(std::move(format));
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return formats;
}

std::string JoinOutputPath(const std::string & base, std::string_view leaf)
{
    if (base.empty())
    {
        return std::string(leaf);
    }
    if (leaf.empty())
    {
        return base;
    }
    if (base.back() == '/')
    {
        return base + std::string(leaf);
    }
    return base + "/" + std::string(leaf);
}

void PrintFormatList()
{
    for (const auto & name : SupportedFormatNames())
    {
        std::cout << name << "\n";
    }
}

struct FormatSelection
{
    std::string name;
    const IFormatDriver * driver = nullptr;
    WriterOptions writer_options;
    OutputLocation output;
};

arrow::Result<std::vector<FormatSelection>> BuildFormatSelections(
    const std::string & format_spec,
    const std::string & output_path,
    const po::variables_map & vm)
{
    auto requested_formats = SplitFormatSpec(format_spec);
    if (requested_formats.empty())
    {
        return arrow::Status::Invalid("output-format must not be empty");
    }
    if (requested_formats.size() == 1 && requested_formats.front() == "all")
    {
        requested_formats = SupportedFormatNames();
    }

    const bool multi_format = requested_formats.size() > 1;
    std::unordered_set<std::string> seen;
    std::vector<FormatSelection> selections;
    selections.reserve(requested_formats.size());
    for (const auto & format_name : requested_formats)
    {
        if (format_name == "all")
        {
            return arrow::Status::Invalid("output-format=all cannot be mixed with explicit format names");
        }
        if (!seen.insert(format_name).second)
        {
            continue;
        }

        ARROW_ASSIGN_OR_RAISE(const IFormatDriver * driver, ResolveFormatDriver(format_name));
        ARROW_ASSIGN_OR_RAISE(WriterOptions writer_options, driver->BuildWriterOptions(vm));

        OutputLocation output;
        output.uri = multi_format ? JoinOutputPath(output_path, driver->Name()) : output_path;
        selections.push_back(FormatSelection{
            .name = std::string(driver->Name()),
            .driver = driver,
            .writer_options = std::move(writer_options),
            .output = std::move(output),
        });
    }

    if (selections.empty())
    {
        return arrow::Status::Invalid("No output formats selected");
    }
    return selections;
}

/*
 * Two-pool design: table_executor holds one future per table (orchestration);
 * part_executor runs partition workers and per-partition pipeline producers.
 * Keeping the pools separate avoids deadlock: table futures block on Get()
 * while waiting for partition workers, so they must not share a pool with
 * those workers.
 */
yaclib::FutureOn<int> LaunchTableTask(
    yaclib::IExecutor & table_executor,
    GenerationContext ctx,
    const TableMetadata & table,
    OutputLocation output,
    WriterOptions writer_options,
    const IFormatDriver * format_driver,
    WriteSchedulerOptions scheduler_options,
    yaclib::IExecutor & part_executor)
{
    return yaclib::Run(
        table_executor,
        [ctx = std::move(ctx),
         &table,
         output = std::move(output),
         writer_options = std::move(writer_options),
         format_driver,
         scheduler_options,
         &part_executor]() mutable -> int {
            return GenerateTableWithStrategy(ctx, table, output, writer_options, *format_driver, scheduler_options, part_executor);
        });
}
}

int main(int argc, char ** argv)
{
    const auto supported_formats = SupportedFormatNames();
    const auto default_output_format = supported_formats.empty() ? std::string("parquet") : supported_formats.front();

    double scale_factor = 1.0;
    std::string output_format = default_output_format;
    std::string output_path = ".";
    std::vector<std::string> tables;
    std::uint64_t batch_rows = 128 * 1024;

    const std::string supported_formats_text = JoinStrings(supported_formats, ", ");
    const std::string output_format_help = supported_formats.empty()
                                               ? "Set tables output format"
                                               : "Set tables output format (" + supported_formats_text + "; comma-separated list or all)";

    po::options_description desc("Allowed options");
    desc.add_options()("help", "Help message")("list-tables", "List available tables")("list-formats", "List available formats")(
        "scale-factor", po::value<double>(&scale_factor)->default_value(1.0), "Set scale factor")(
        "output-format",
        po::value<std::string>(&output_format)->default_value(default_output_format),
        output_format_help.c_str())(
        "output-path", po::value<std::string>(&output_path)->default_value("."), "Set output directory path")(
        "table", po::value<std::vector<std::string>>(&tables)->multitoken(), "Specify tables to generate")(
        "batch-rows", po::value<std::uint64_t>(&batch_rows)->default_value(128 * 1024), "Control rows per batch count");
    RegisterFormatCliOptions(desc);
    RegisterWriteSchedulerCliOptions(desc);
    RegisterProgressCliOptions(desc);

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const std::exception & ex)
    {
        std::cerr << "Invalid arguments: " << ex.what() << "\n";
        std::cerr << desc << "\n";
        return 2;
    }

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        return 0;
    }
    if (vm.count("list-tables"))
    {
        PrintTableList();
        return 0;
    }
    if (vm.count("list-formats"))
    {
        PrintFormatList();
        return 0;
    }

    if (scale_factor <= 0.0)
    {
        std::cerr << "Scale factor must be > 0\n";
        return 2;
    }
    if (batch_rows == 0)
    {
        std::cerr << "Batch rows must be > 0\n";
        return 2;
    }
    RegisterTables();
    auto & registry = TableRegistry::Instance();

    if (tables.empty())
    {
        tables = DefaultTableNames();
    }
    else if (tables.size() == 1 && tables.front() == "all")
    {
        tables = DefaultTableNames();
    }

    auto format_selections_result = BuildFormatSelections(output_format, output_path, vm);
    if (!format_selections_result.ok())
    {
        std::cerr << "Invalid output format: " << format_selections_result.status().ToString() << "\n";
        return 2;
    }
    auto format_selections = std::move(format_selections_result).ValueOrDie();

    auto scheduler_options_result = BuildWriteSchedulerOptions(vm);
    if (!scheduler_options_result.ok())
    {
        std::cerr << "Invalid write scheduler options: " << scheduler_options_result.status().ToString() << "\n";
        return 2;
    }
    const WriteSchedulerOptions scheduler_options = std::move(scheduler_options_result).ValueOrDie();

    auto verbosity_result = BuildCliVerbosity(vm);
    if (!verbosity_result.ok())
    {
        std::cerr << verbosity_result.status().ToString() << "\n";
        return 2;
    }
    const CliVerbosity verbosity = std::move(verbosity_result).ValueOrDie();

    const ScaleConfig scale{.factor = scale_factor};
    arrow::MemoryPool * pool = arrow::default_memory_pool();
    static const TextPool & text_pool = TextPool::Default();
    auto table_executor = yaclib::MakeFairThreadPool();
    auto part_executor = yaclib::MakeFairThreadPool();

    GenerationContext ctx{
        .registry = &registry,
        .text_pool = &text_pool,
        .scale = &scale,
        .pool = pool,
        .batch_rows = batch_rows,
    };

    std::vector<const TableMetadata *> selected_tables;
    selected_tables.reserve(tables.size());
    for (const auto & table_name : tables)
    {
        const auto * metadata = registry.FindMetadata(table_name);
        if (!metadata)
        {
            std::cerr << "Unknown table: " << table_name << "\n";
            table_executor->SoftStop();
            table_executor->Wait();
            part_executor->SoftStop();
            part_executor->Wait();
            return 2;
        }
        selected_tables.push_back(metadata);
    }

    /*
     * Longest-processing-time (LPT) ordering: schedule the biggest tables
     * first so they keep workers busy as small tables (region, nation,
     * supplier) drain quickly. This reduces makespan when several tables
     * are generated together.
     */
    std::stable_sort(
        selected_tables.begin(),
        selected_tables.end(),
        [&scale](const TableMetadata * lhs, const TableMetadata * rhs) {
            return scale.RowCount(*lhs) > scale.RowCount(*rhs);
        });

    std::unique_ptr<CliProgressController> progress_controller;
    if (verbosity == CliVerbosity::Verbose)
    {
        std::vector<CliProgressTable> progress_tables;
        progress_tables.reserve(selected_tables.size() * format_selections.size());
        for (const auto & format : format_selections)
        {
            for (const auto * metadata : selected_tables)
            {
                const auto table_label = format_selections.size() == 1 ? metadata->name : format.name + "/" + metadata->name;
                progress_tables.push_back(CliProgressTable{
                    .table_name = table_label,
                    .total_rows = scale.RowCount(*metadata),
                });
            }
        }

        CliProgressControllerOptions progress_options;
        progress_options.verbosity = verbosity;
        progress_options.stream = &std::cerr;
        progress_options.is_tty = ::isatty(STDERR_FILENO) == 1;
        progress_controller = std::make_unique<CliProgressController>(std::move(progress_tables), std::move(progress_options));
    }

    int exit_code = 0;
    std::size_t global_task_index = 0;
    std::vector<std::pair<std::size_t, yaclib::FutureOn<int>>> table_futures;
    table_futures.reserve(format_selections.size() * selected_tables.size());

    for (std::size_t format_index = 0; format_index < format_selections.size(); ++format_index)
    {
        const auto & format = format_selections[format_index];
        for (std::size_t table_index = 0; table_index < selected_tables.size(); ++table_index)
        {
            GenerationContext table_ctx = ctx;
            table_ctx.progress = progress_controller.get();
            table_ctx.progress_table_index = format_index * selected_tables.size() + table_index;
            table_futures.emplace_back(
                global_task_index,
                LaunchTableTask(
                    *table_executor,
                    std::move(table_ctx),
                    *selected_tables[table_index],
                    format.output,
                    format.writer_options,
                    format.driver,
                    scheduler_options,
                    *part_executor));
            ++global_task_index;
        }
    }

    for (auto & [task_index, future] : table_futures)
    {
        auto result = std::move(future).Get();
        try
        {
            const int code = std::move(result).Ok();
            if (code != 0)
            {
                exit_code = code;
            }
        }
        catch (const std::exception & ex)
        {
            const std::string message = "Table task failed: " + std::string(ex.what());
            if (progress_controller)
            {
                progress_controller->ReportFailure(task_index, message);
            }
            else
            {
                std::cerr << message << "\n";
            }
            exit_code = 2;
        }
    }

    table_executor->SoftStop();
    table_executor->Wait();
    part_executor->SoftStop();
    part_executor->Wait();

    const auto finalize_status = FinalizeStorageBackends();
    if (!finalize_status.ok())
    {
        std::cerr << "Failed to finalize storage backends: " << finalize_status.ToString() << "\n";
        if (exit_code == 0)
        {
            exit_code = 2;
        }
    }

    return exit_code;
}
