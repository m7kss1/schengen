#include "Common/partition.h"
#include "Common/rands.h"
#include "Common/registry.h"
#include "Storage/writer.h"
#include "Tables/register_tables.h"
#include "yaclib/async/future.hpp"
#include "yaclib/async/run.hpp"
#include "yaclib/runtime/fair_thread_pool.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace po = boost::program_options;

static std::vector<std::string> DefaultTableNames()
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

static void PrintTableList()
{
    for (const auto & name : DefaultTableNames())
    {
        std::cout << name << "\n";
    }
}

static std::int32_t ResolveParquetPartCount(
    const TableMetadata & table,
    const ScaleConfig & scale,
    const WriterOptions & options)
{
    const auto row_group_rows =
        ParquetTableWriter::ResolveRowGroupRows(table, options.parquet);
    if (row_group_rows <= 0)
    {
        return 1;
    }

    const auto total_rows = scale.RowCount(table);
    const auto row_group_rows_u =
        static_cast<std::uint64_t>(row_group_rows);
    const auto parts_u =
        (total_rows + row_group_rows_u - 1) / row_group_rows_u;
    const auto max_parts =
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    return static_cast<std::int32_t>(std::min(parts_u, max_parts));
}

struct GenerationContext
{
    const TableRegistry * registry = nullptr;
    const TextPool * text_pool = nullptr;
    const ScaleConfig * scale = nullptr;
    arrow::MemoryPool * pool = nullptr;
    std::uint64_t batch_rows = 0;
};

struct PartResult
{
    std::vector<TableBatch> batches;
    std::string error;
};

static PartResult GeneratePartition(
    const GenerationContext & ctx,
    const TableMetadata & table,
    std::int32_t part,
    std::int32_t part_count)
{
    PartResult result;
    try
    {
        auto generator = ctx.registry->CreateGenerator(table.name, ctx.pool);
        if (!generator)
        {
            result.error = "Failed to create generator for table: " + table.name;
            return result;
        }

        GeneratorContext gen_ctx;
        gen_ctx.scale = *ctx.scale;
        gen_ctx.partition = MakePartitionPlan(table, *ctx.scale, part, part_count);
        gen_ctx.text_pool = ctx.text_pool;
        gen_ctx.pool = ctx.pool;
        generator->Reset(gen_ctx);

        const auto part_rows =
            gen_ctx.partition.range.end_row - gen_ctx.partition.range.start_row;
        if (part_rows > 0)
        {
            const auto expected_batches =
                (part_rows + ctx.batch_rows - 1) / ctx.batch_rows;
            result.batches.reserve(static_cast<std::size_t>(expected_batches));
        }

        TableBatch batch;
        while (generator->NextBatch(ctx.batch_rows, &batch))
        {
            /* Move batch buffers to keep zero-copy semantics. */
            result.batches.emplace_back(std::move(batch));
            batch = {};
        }
    }
    catch (const std::exception & ex)
    {
        result.error = ex.what();
    }
    catch (...)
    {
        result.error = "Unknown error during partition generation";
    }
    return result;
}

static yaclib::FutureOn<PartResult> LaunchPartitionTask(
    yaclib::IExecutor & executor,
    const GenerationContext & ctx,
    const TableMetadata & table,
    std::int32_t part,
    std::int32_t part_count)
{
    return yaclib::Run(
        executor,
        [&ctx, &table, part, part_count]() { return GeneratePartition(ctx, table, part, part_count); });
}

static int GenerateTable(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    yaclib::IExecutor & part_executor)
{
    const auto part_count =
        ResolveParquetPartCount(table, *ctx.scale, writer_options);

    std::vector<yaclib::FutureOn<PartResult>> part_futures;
    part_futures.reserve(static_cast<std::size_t>(part_count));
    for (std::int32_t part = 1; part <= part_count; ++part)
    {
        part_futures.emplace_back(
            LaunchPartitionTask(part_executor, ctx, table, part, part_count));
    }

    auto writer = MakeTableWriter(writer_options.format);
    if (!writer)
    {
        std::cerr << "Writer not available for table: " << table.name << "\n";
        return 2;
    }

    const auto open_status = writer->Open(table, output, /*part_num=*/1, writer_options, ctx.pool);
    if (!open_status.ok())
    {
        std::cerr << "Writer open failed: " << open_status.ToString() << "\n";
        return 2;
    }

    for (std::int32_t part = 1; part <= part_count; ++part)
    {
        auto part_result = std::move(part_futures[part - 1]).Get();
        PartResult payload;
        try
        {
            payload = std::move(part_result).Ok();
        }
        catch (const std::exception & ex)
        {
            std::cerr << "Partition " << part << " failed: " << ex.what() << "\n";
            return 2;
        }
        if (!payload.error.empty())
        {
            std::cerr << "Partition " << part << " failed: " << payload.error << "\n";
            return 2;
        }
        if (payload.batches.empty())
        {
            continue;
        }

        const auto begin_status = writer->BeginRowGroup();
        if (!begin_status.ok())
        {
            std::cerr << "Failed to begin row group: " << begin_status.ToString() << "\n";
            return 2;
        }

        for (const auto & batch : payload.batches)
        {
            const auto write_status = writer->WriteBatch(batch);
            if (!write_status.ok())
            {
                std::cerr << "Failed to write data: " << write_status.ToString() << "\n";
                return 2;
            }
        }
    }

    const auto close_status = writer->Close();
    if (!close_status.ok())
    {
        std::cerr << "Failed to close writer: " << close_status.ToString() << "\n";
        return 2;
    }

    return 0;
}

static yaclib::FutureOn<int> LaunchTableTask(
    yaclib::IExecutor & table_executor,
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    yaclib::IExecutor & part_executor)
{
    return yaclib::Run(
        table_executor,
        [&ctx, &table, &output, &writer_options, &part_executor]() {
            return GenerateTable(ctx, table, output, writer_options, part_executor);
        });
}

int main(int argc, char ** argv)
{
    double scale_factor = 1.0;
    std::string output_format = "parquet";
    std::string output_path = ".";
    std::vector<std::string> tables;
    std::uint64_t batch_rows = 128 * 1024;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Help message")
        ("list-tables", "List available tables")
        ("scale-factor", po::value<double>(&scale_factor)->default_value(1.0), "Set scale factor")
        ("output-format", po::value<std::string>(&output_format)->default_value("parquet"), "Set tables output format")
        ("output-path", po::value<std::string>(&output_path)->default_value("."), "Set output directory path")
        ("table", po::value<std::vector<std::string>>(&tables)->multitoken(), "Specify tables to generate")
        ("batch-rows", po::value<std::uint64_t>(&batch_rows)->default_value(128 * 1024), "Control rows per batch count")
    ;

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

    /* TODO: FIXME */
    OutputFormat format = OutputFormat::Parquet;
    if (output_format != "parquet")
    {
        std::cerr << "Unsupported output format: " << output_format << "\n";
        return 2;
    }

    const ScaleConfig scale{.factor = scale_factor};
    const OutputLocation output{.uri = output_path};
    WriterOptions writer_options;
    writer_options.format = format;

    arrow::MemoryPool * pool = arrow::default_memory_pool();
    static const TextPool & text_pool = TextPool::Default();
    /* Separate executors avoid blocking tasks waiting on work scheduled to the same pool */
    auto table_executor = yaclib::MakeFairThreadPool();
    auto part_executor = yaclib::MakeFairThreadPool();

    GenerationContext ctx{
        .registry = &registry,
        .text_pool = &text_pool,
        .scale = &scale,
        .pool = pool,
        .batch_rows = batch_rows,
    };

    std::vector<yaclib::FutureOn<int>> table_futures;
    table_futures.reserve(tables.size());
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

        table_futures.emplace_back(
            LaunchTableTask(
                *table_executor,
                ctx,
                *metadata,
                output,
                writer_options,
                *part_executor));
    }

    int exit_code = 0;
    for (auto & future : table_futures)
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
            std::cerr << "Table task failed: " << ex.what() << "\n";
            exit_code = 2;
        }
    }

    table_executor->SoftStop();
    table_executor->Wait();
    part_executor->SoftStop();
    part_executor->Wait();

    return exit_code;
}
