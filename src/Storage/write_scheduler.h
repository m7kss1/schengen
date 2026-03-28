#pragma once

#include "Storage/writer.h"

#include "Common/partition.h"
#include "Common/rands.h"
#include "Common/registry.h"

#include <arrow/result.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace boost::program_options
{
class options_description;
class variables_map;
} // namespace boost::program_options

namespace yaclib
{
class IExecutor;
} // namespace yaclib

class CliProgressController;

struct WriteSchedulerOptions
{
    std::optional<WriteStrategy> strategy;
    std::uint32_t worker_count = 0;
    std::size_t queue_capacity = 8;
};

struct GenerationContext
{
    const TableRegistry * registry = nullptr;
    const TextPool * text_pool = nullptr;
    const ScaleConfig * scale = nullptr;
    arrow::MemoryPool * pool = nullptr;
    std::uint64_t batch_rows = 0;
    CliProgressController * progress = nullptr;
    std::size_t progress_table_index = std::numeric_limits<std::size_t>::max();
};

void RegisterWriteSchedulerCliOptions(boost::program_options::options_description & desc);
arrow::Result<WriteSchedulerOptions> BuildWriteSchedulerOptions(const boost::program_options::variables_map & vm);
arrow::Result<WriteStrategy> ResolveSelectedWriteStrategy(
    const WriteSchedulerOptions & options,
    const IFormatDriver & format_driver);

int GenerateTableWithStrategy(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor);
