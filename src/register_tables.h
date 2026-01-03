#pragma once

#include "region.h"
#include "table_registry.h"

inline void RegisterTableRegion(TableRegistry & registry)
{
    registry.RegisterTable(
        kRegion,
        [](arrow::MemoryPool * pool) {
            return std::make_unique<RegionGenerator>(pool);
        });
}

inline void RegisterTables()
{
    auto & registry = TableRegistry::Instance();

    /* Register TPC-H tables */
    {
        RegisterTableRegion(registry);
    }
}
