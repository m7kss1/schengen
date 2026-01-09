#pragma once

#include "customer.h"
#include "lineitem.h"
#include "nation.h"
#include "orders.h"
#include "part.h"
#include "partsupp.h"
#include "region.h"
#include "supplier.h"
#include "table_registry.h"

inline void RegisterTableCustomer(TableRegistry & registry)
{
    registry.RegisterTable(kCustomer, [](arrow::MemoryPool * pool) { return std::make_unique<CustomerGenerator>(pool); });
}

inline void RegisterTableNation(TableRegistry & registry)
{
    registry.RegisterTable(kNation, [](arrow::MemoryPool * pool) { return std::make_unique<NationGenerator>(pool); });
}

inline void RegisterTableRegion(TableRegistry & registry)
{
    registry.RegisterTable(kRegion, [](arrow::MemoryPool * pool) { return std::make_unique<RegionGenerator>(pool); });
}

inline void RegisterTableSupplier(TableRegistry & registry)
{
    registry.RegisterTable(kSupplier, [](arrow::MemoryPool * pool) { return std::make_unique<SupplierGenerator>(pool); });
}

inline void RegisterTablePartSupp(TableRegistry & registry)
{
    registry.RegisterTable(kPartsupp, [](arrow::MemoryPool * pool) { return std::make_unique<PartSuppGenerator>(pool); });
}

inline void RegisterTablePart(TableRegistry & registry)
{
    registry.RegisterTable(kPart, [](arrow::MemoryPool * pool) { return std::make_unique<PartGenerator>(pool); });
}

inline void RegisterTableOrders(TableRegistry & registry)
{
    registry.RegisterTable(kOrders, [](arrow::MemoryPool * pool) { return std::make_unique<OrderGenerator>(pool); });
}

inline void RegisterTableLineitem(TableRegistry & registry)
{
    registry.RegisterTable(kLineitem, [](arrow::MemoryPool * pool) { return std::make_unique<LineitemGenerator>(pool); });
}

inline void RegisterTables()
{
    auto & registry = TableRegistry::Instance();

    /* Register TPC-H tables */
    {
        RegisterTableCustomer(registry);
        RegisterTableNation(registry);
        RegisterTableRegion(registry);
        RegisterTableSupplier(registry);
        RegisterTablePartSupp(registry);
        RegisterTablePart(registry);
        RegisterTableOrders(registry);
        RegisterTableLineitem(registry);
    }
}
