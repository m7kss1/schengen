#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Common/generator.h"

class TableRegistry
{
public:
    using GeneratorFactory = std::function<std::unique_ptr<ITableGenerator>(arrow::MemoryPool *)>;

    static TableRegistry & Instance();

    void RegisterTable(const TableMetadata & metadata, GeneratorFactory factory);

    const TableMetadata * FindMetadata(std::string_view name) const;

    std::unique_ptr<ITableGenerator> CreateGenerator(std::string_view name, arrow::MemoryPool * pool) const;

private:
    struct Entry
    {
        const TableMetadata * metadata = nullptr;
        GeneratorFactory factory;
    };

    std::unordered_map<std::string, Entry> tables_;
};
