#include <algorithm>
#include <cstdint>

#include "benchmark/benchmark.h"

#include "Common/rands.h"
#include "Tables/customer.h"
#include "Tables/lineitem.h"
#include "Tables/nation.h"
#include "Tables/orders.h"
#include "Tables/part.h"
#include "Tables/partsupp.h"
#include "Tables/region.h"
#include "Tables/supplier.h"
#include "Tables/table.h"

namespace
{

constexpr std::uint64_t kRowCap = 128ULL * 1024ULL;
constexpr double kScaleFactor = 10.0;
constexpr std::uint64_t kSuppliersPerPart = 4;
constexpr std::uint64_t kApproxLineitemsPerOrder = 4;

void BM_RegionIterator(benchmark::State & state)
{
    RegionRowIterator iterator;
    RegionRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = kRegion.rows_at_sf1;

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_NationIterator(benchmark::State & state)
{
    NationRowIterator iterator;
    NationRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = kNation.rows_at_sf1;

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_SupplierIterator(benchmark::State & state)
{
    SupplierRowIterator iterator;
    SupplierRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = std::min<std::uint64_t>(kSupplier.rows_at_sf1, kRowCap);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_PartIterator(benchmark::State & state)
{
    PartRowIterator iterator;
    PartRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = std::min<std::uint64_t>(kPart.rows_at_sf1, kRowCap);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, text_pool);
        while (iterator.NextValue(&row))
        {
        }
    }
}

void BM_PartSuppIterator(benchmark::State & state)
{
    PartSuppRowIterator iterator;
    PartSuppRow row{};
    const TextPool & text_pool = TextPool::Default();
    std::uint64_t part_limit = std::max<std::uint64_t>(1, kRowCap / kSuppliersPerPart);
    part_limit = std::min<std::uint64_t>(part_limit, kPart.rows_at_sf1);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, part_limit, kScaleFactor, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_CustomerIterator(benchmark::State & state)
{
    CustomerRowIterator iterator;
    CustomerRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = std::min<std::uint64_t>(kCustomer.rows_at_sf1, kRowCap);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_OrdersIterator(benchmark::State & state)
{
    OrderRowIterator iterator;
    OrderRow row{};
    const TextPool & text_pool = TextPool::Default();
    const std::uint64_t row_limit = std::min<std::uint64_t>(kOrders.rows_at_sf1, kRowCap);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, row_limit, kScaleFactor, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

void BM_LineitemIterator(benchmark::State & state)
{
    LineitemRowIterator iterator;
    LineitemRow row{};
    const TextPool & text_pool = TextPool::Default();
    std::uint64_t order_limit = std::max<std::uint64_t>(1, kRowCap / kApproxLineitemsPerOrder);
    order_limit = std::min<std::uint64_t>(order_limit, kOrders.rows_at_sf1);

    for (auto _ : state)
    {
        (void)_;
        iterator.Reset(0, order_limit, kScaleFactor, text_pool);
        while (iterator.Next(&row))
        {
        }
    }
}

} // namespace

BENCHMARK(BM_RegionIterator);
BENCHMARK(BM_NationIterator);
BENCHMARK(BM_SupplierIterator);
BENCHMARK(BM_PartIterator);
BENCHMARK(BM_PartSuppIterator);
BENCHMARK(BM_CustomerIterator);
BENCHMARK(BM_OrdersIterator);
BENCHMARK(BM_LineitemIterator);

BENCHMARK_MAIN();


/*
II) Scale factor: 10.0
Run on (8 X 2199.99 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 64 KiB (x4)
  L2 Unified 512 KiB (x4)
  L3 Unified 8192 KiB (x1)
Load Average: 0.59, 0.72, 1.00
--------------------------------------------------------------
Benchmark                    Time             CPU   Iterations
--------------------------------------------------------------
BM_RegionIterator          160 ns          160 ns      4428913
BM_NationIterator          518 ns          518 ns      1366342
BM_SupplierIterator    8829157 ns      8829224 ns           79
BM_PartIterator      104526532 ns    104529057 ns            7
BM_PartSuppIterator    4869497 ns      4869766 ns          143
BM_CustomerIterator   86499412 ns     86507480 ns            8
BM_OrdersIterator     72801355 ns     72806595 ns           10
BM_LineitemIterator   35420631 ns     35423063 ns           20
*/