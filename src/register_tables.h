#pragma once

#include "nation.h"
#include "region.h"
#include "table_registry.h"

inline void RegisterTableNation(TableRegistry & registry)
{
    registry.RegisterTable(
        kNation,
        [](arrow::MemoryPool * pool) {
            return std::make_unique<NationGenerator>(pool);
        });
}

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
        RegisterTableNation(registry);
        RegisterTableRegion(registry);
    }
}
