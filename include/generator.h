#pragma once

class TableBatch;
class TableMetadata;

/* 
 * Interface for random number generators 
 */
class ITableGenerator
{
public:
    virtual ~ITableGenerator() = default;

    /* Provide static schema information about the table */
    virtual const TableMetadata & GetTableMetadata() const = 0;

    /* Generate all rows for current partition */
    virtual TableBatch GenerateBatch() = 0;
};
