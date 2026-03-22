#include "Common/registry.h"
#include "Storage/progress.h"
#include "Storage/write_scheduler.h"
#include "Storage/writer.h"
#include "Tables/register_tables.h"
#include "yaclib/async/future.hpp"
#include "yaclib/async/run.hpp"
#include "yaclib/runtime/fair_thread_pool.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
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

void PrintFormatList()
{
    for (const auto & name : SupportedFormatNames())
    {
        std::cout << name << "\n";
    }
}

yaclib::FutureOn<int> LaunchTableTask(
    yaclib::IExecutor & table_executor,
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver * format_driver,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    return yaclib::Run(
        table_executor,
        [&ctx, &table, &output, &writer_options, format_driver, &scheduler_options, &part_executor]() {
            return GenerateTableWithStrategy(
                ctx, table, output, writer_options, *format_driver, scheduler_options, part_executor);
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
    bool progress_json = false;

    const std::string supported_formats_text = JoinStrings(supported_formats, ", ");
    const std::string output_format_help = supported_formats.empty()
                                               ? "Set tables output format"
                                               : "Set tables output format (" + supported_formats_text + ")";

    po::options_description desc("Allowed options");
    desc.add_options()("help", "Help message")("list-tables", "List available tables")("list-formats", "List available formats")(
        "scale-factor", po::value<double>(&scale_factor)->default_value(1.0), "Set scale factor")(
        "output-format",
        po::value<std::string>(&output_format)->default_value(default_output_format),
        output_format_help.c_str())(
        "output-path", po::value<std::string>(&output_path)->default_value("."), "Set output directory path")(
        "table", po::value<std::vector<std::string>>(&tables)->multitoken(), "Specify tables to generate")(
        "batch-rows", po::value<std::uint64_t>(&batch_rows)->default_value(128 * 1024), "Control rows per batch count")(
        "progress-json", po::bool_switch(&progress_json), "Emit machine-readable progress events to stdout as NDJSON");
    RegisterFormatCliOptions(desc);
    RegisterWriteSchedulerCliOptions(desc);

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

    auto format_driver_result = ResolveFormatDriver(output_format);
    if (!format_driver_result.ok())
    {
        std::cerr << format_driver_result.status().ToString() << "\n";
        return 2;
    }
    const IFormatDriver * format_driver = std::move(format_driver_result).ValueOrDie();

    auto writer_options_result = format_driver->BuildWriterOptions(vm);
    if (!writer_options_result.ok())
    {
        std::cerr << "Invalid writer options: " << writer_options_result.status().ToString() << "\n";
        return 2;
    }
    WriterOptions writer_options = std::move(writer_options_result).ValueOrDie();

    auto scheduler_options_result = BuildWriteSchedulerOptions(vm);
    if (!scheduler_options_result.ok())
    {
        std::cerr << "Invalid write scheduler options: " << scheduler_options_result.status().ToString() << "\n";
        return 2;
    }
    const WriteSchedulerOptions scheduler_options = std::move(scheduler_options_result).ValueOrDie();

    const ScaleConfig scale{.factor = scale_factor};
    const OutputLocation output{.uri = output_path};
    const std::string resolved_format_name(format_driver->Name());

    arrow::MemoryPool * pool = arrow::default_memory_pool();
    static const TextPool & text_pool = TextPool::Default();
    auto table_executor = yaclib::MakeFairThreadPool();
    auto part_executor = yaclib::MakeFairThreadPool();
    JsonProgressSink json_progress_sink(std::cout);
    IProgressSink * progress_sink = progress_json ? static_cast<IProgressSink *>(&json_progress_sink) : nullptr;

    GenerationContext ctx{
        .registry = &registry,
        .text_pool = &text_pool,
        .scale = &scale,
        .pool = pool,
        .batch_rows = batch_rows,
        .format_name = resolved_format_name,
        .progress = progress_sink,
    };

    if (progress_sink != nullptr)
    {
        progress_sink->RunStarted(RunStartedEvent{
            .selected_formats = {resolved_format_name},
            .scale_factor = scale_factor,
            .output_path = output_path,
        });
        progress_sink->FormatStarted(FormatStartedEvent{
            .format = resolved_format_name,
            .index = 1,
            .total_formats = 1,
        });
    }

    std::vector<yaclib::FutureOn<int>> table_futures;
    std::vector<std::string> table_task_names;
    table_futures.reserve(tables.size());
    table_task_names.reserve(tables.size());
    for (const auto & table_name : tables)
    {
        const auto * metadata = registry.FindMetadata(table_name);
        if (!metadata)
        {
            std::cerr << "Unknown table: " << table_name << "\n";
            if (progress_sink != nullptr)
            {
                progress_sink->FormatFinished(FormatFinishedEvent{.format = resolved_format_name, .success = false});
                progress_sink->RunFinished(RunFinishedEvent{.success = false});
            }
            table_executor->SoftStop();
            table_executor->Wait();
            part_executor->SoftStop();
            part_executor->Wait();
            return 2;
        }

        table_futures.emplace_back(LaunchTableTask(
            *table_executor, ctx, *metadata, output, writer_options, format_driver, scheduler_options, *part_executor));
        table_task_names.emplace_back(table_name);
    }

    int exit_code = 0;
    for (std::size_t i = 0; i < table_futures.size(); ++i)
    {
        auto result = std::move(table_futures[i]).Get();
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
            std::cerr << "Table task failed: " << ex.what() << "\n";
            if (progress_sink != nullptr)
            {
                progress_sink->TableFailed(TableFailedEvent{
                    .format = resolved_format_name,
                    .table = table_task_names[i],
                    .error = ex.what(),
                });
            }
            exit_code = 2;
        }
    }

    table_executor->SoftStop();
    table_executor->Wait();
    part_executor->SoftStop();
    part_executor->Wait();

    if (progress_sink != nullptr)
    {
        const bool success = exit_code == 0;
        progress_sink->FormatFinished(FormatFinishedEvent{.format = resolved_format_name, .success = success});
        progress_sink->RunFinished(RunFinishedEvent{.success = success});
    }

    return exit_code;
}
