#include "Common/registry.h"

TableRegistry & TableRegistry::Instance()
{
    static TableRegistry instance;
    return instance;
}

void TableRegistry::RegisterTable(const TableMetadata & metadata, GeneratorFactory factory)
{
    const std::string key = metadata.name;
    tables_.emplace(key, Entry{&metadata, std::move(factory)});
}

const TableMetadata * TableRegistry::FindMetadata(std::string_view name) const
{
    const auto it = tables_.find(std::string(name));
    if (it == tables_.end())
    {
        return nullptr;
    }
    return it->second.metadata;
}

std::unique_ptr<ITableGenerator> TableRegistry::CreateGenerator(std::string_view name, arrow::MemoryPool * pool) const
{
    const auto it = tables_.find(std::string(name));
    if (it == tables_.end())
    {
        return nullptr;
    }
    return it->second.factory(pool);
}
