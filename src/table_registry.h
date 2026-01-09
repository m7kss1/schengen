#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "generator.h"

class TableRegistry
{
public:
    using GeneratorFactory = std::function<std::unique_ptr<ITableGenerator>(arrow::MemoryPool *)>;

    static TableRegistry & Instance()
    {
        static TableRegistry instance;
        return instance;
    }

    void RegisterTable(const TableMetadata & metadata, GeneratorFactory factory)
    {
        const std::string key = metadata.name;
        tables_.emplace(key, Entry{&metadata, std::move(factory)});
    }

    const TableMetadata * FindMetadata(std::string_view name) const
    {
        const auto it = tables_.find(std::string(name));
        if (it == tables_.end())
        {
            return nullptr;
        }
        return it->second.metadata;
    }

    std::unique_ptr<ITableGenerator> CreateGenerator(std::string_view name, arrow::MemoryPool * pool) const
    {
        const auto it = tables_.find(std::string(name));
        if (it == tables_.end())
        {
            return nullptr;
        }
        return it->second.factory(pool);
    }

private:
    struct Entry
    {
        const TableMetadata * metadata = nullptr;
        GeneratorFactory factory;
    };

    std::unordered_map<std::string, Entry> tables_;
};
