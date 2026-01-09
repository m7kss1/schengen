#pragma once

#include "Tables/customer.h"
#include "Tables/lineitem.h"
#include "Tables/nation.h"
#include "Tables/orders.h"
#include "Tables/part.h"
#include "Tables/partsupp.h"
#include "Tables/region.h"
#include "Tables/supplier.h"
#include "Common/registry.h"

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
