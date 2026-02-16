#include "Tables/register_tables.h"

void RegisterTableCustomer(TableRegistry & registry)
{
    registry.RegisterTable(kCustomer, [](arrow::MemoryPool * pool) { return std::make_unique<CustomerGenerator>(pool); });
}

void RegisterTableNation(TableRegistry & registry)
{
    registry.RegisterTable(kNation, [](arrow::MemoryPool * pool) { return std::make_unique<NationGenerator>(pool); });
}

void RegisterTableRegion(TableRegistry & registry)
{
    registry.RegisterTable(kRegion, [](arrow::MemoryPool * pool) { return std::make_unique<RegionGenerator>(pool); });
}

void RegisterTableSupplier(TableRegistry & registry)
{
    registry.RegisterTable(kSupplier, [](arrow::MemoryPool * pool) { return std::make_unique<SupplierGenerator>(pool); });
}

void RegisterTablePartSupp(TableRegistry & registry)
{
    registry.RegisterTable(kPartsupp, [](arrow::MemoryPool * pool) { return std::make_unique<PartSuppGenerator>(pool); });
}

void RegisterTablePart(TableRegistry & registry)
{
    registry.RegisterTable(kPart, [](arrow::MemoryPool * pool) { return std::make_unique<PartGenerator>(pool); });
}

void RegisterTableOrders(TableRegistry & registry)
{
    registry.RegisterTable(kOrders, [](arrow::MemoryPool * pool) { return std::make_unique<OrderGenerator>(pool); });
}

void RegisterTableLineitem(TableRegistry & registry)
{
    registry.RegisterTable(kLineitem, [](arrow::MemoryPool * pool) { return std::make_unique<LineitemGenerator>(pool); });
}

void RegisterTables()
{
    auto & registry = TableRegistry::Instance();

    RegisterTableCustomer(registry);
    RegisterTableNation(registry);
    RegisterTableRegion(registry);
    RegisterTableSupplier(registry);
    RegisterTablePartSupp(registry);
    RegisterTablePart(registry);
    RegisterTableOrders(registry);
    RegisterTableLineitem(registry);
}
