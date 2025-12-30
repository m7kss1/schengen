#pragma once

#include "partition.h"

#include <cstdint>
#include <arrow/api.h>

class DictionarySet;

struct GeneratorContext
{
    ScaleConfig scale;
    PartitionPlan partition;
    /* Contains distributions and text pool for tables */
    const DictionarySet * dictionaries = nullptr;
    arrow::MemoryPool * pool = nullptr;
};

/* Interface for table data generators */
class ITableGenerator
{
public:
    virtual ~ITableGenerator() = default;
    virtual const TableMetadata & GetTableMetadata() const = 0;

    virtual void Reset(const GeneratorContext & ctx) = 0;

    /*
     * Stream batches in current partition
     * Return false if partition is done
     */
    virtual bool NextBatch(std::uint64_t max_rows, TableBatch * out) = 0;
};