#include "Tables/table.h"

VarcharColumnBuilder::VarcharColumnBuilder(arrow::MemoryPool * pool)
    : builder_(std::make_unique<arrow::StringBuilder>(pool))
{
}

void VarcharColumnBuilder::Reset()
{
    builder_->Reset();
}

void VarcharColumnBuilder::Append(std::string_view value)
{
    const auto status = builder_->Append(value);
    if (!status.ok())
    {
        throw std::runtime_error(status.ToString());
    }
}

std::shared_ptr<arrow::Array> VarcharColumnBuilder::Finish()
{
    std::shared_ptr<arrow::Array> array;
    const auto status = builder_->Finish(&array);
    if (!status.ok())
    {
        throw std::runtime_error(status.ToString());
    }
    return array;
}

arrow::StringBuilder * VarcharColumnBuilder::builder()
{
    return builder_.get();
}

const arrow::StringBuilder * VarcharColumnBuilder::builder() const
{
    return builder_.get();
}

BuilderFactory::BuilderFactory(arrow::MemoryPool * pool)
    : pool_(pool)
{
}

VarcharColumnBuilder BuilderFactory::CreateVarchar() const
{
    return VarcharColumnBuilder(pool_);
}

arrow::MemoryPool * BuilderFactory::pool() const
{
    return pool_;
}

CustomerColumns::CustomerColumns(const BuilderFactory & factory)
    : c_custkey(factory.CreateNumeric<arrow::Int32Type>())
    , c_name(factory.CreateVarchar())
    , c_address(factory.CreateVarchar())
    , c_nationkey(factory.CreateNumeric<arrow::Int32Type>())
    , c_phone(factory.CreateVarchar())
    , c_acctbal(factory.CreateNumeric<arrow::DoubleType>())
    , c_mktsegment(factory.CreateVarchar())
    , c_comment(factory.CreateVarchar())
{
}

void CustomerColumns::ClearAll()
{
    ResetAll(c_custkey, c_name, c_address, c_nationkey, c_phone, c_acctbal, c_mktsegment, c_comment);
}

OrdersColumns::OrdersColumns(const BuilderFactory & factory)
    : o_orderkey(factory.CreateNumeric<arrow::Int32Type>())
    , o_custkey(factory.CreateNumeric<arrow::Int32Type>())
    , o_orderstatus(factory.CreateVarchar())
    , o_totalprice(factory.CreateNumeric<arrow::DoubleType>())
    , o_orderdate(factory.CreateNumeric<arrow::Date32Type>())
    , o_orderpriority(factory.CreateVarchar())
    , o_clerk(factory.CreateVarchar())
    , o_shippriority(factory.CreateNumeric<arrow::Int32Type>())
    , o_comment(factory.CreateVarchar())
{
}

void OrdersColumns::ClearAll()
{
    ResetAll(o_orderkey, o_custkey, o_orderstatus, o_totalprice, o_orderdate, o_orderpriority, o_clerk, o_shippriority, o_comment);
}

LineitemColumns::LineitemColumns(const BuilderFactory & factory)
    : l_orderkey(factory.CreateNumeric<arrow::Int32Type>())
    , l_partkey(factory.CreateNumeric<arrow::Int32Type>())
    , l_suppkey(factory.CreateNumeric<arrow::Int32Type>())
    , l_linenumber(factory.CreateNumeric<arrow::Int32Type>())
    , l_quantity(factory.CreateNumeric<arrow::DoubleType>())
    , l_extendedprice(factory.CreateNumeric<arrow::DoubleType>())
    , l_discount(factory.CreateNumeric<arrow::DoubleType>())
    , l_tax(factory.CreateNumeric<arrow::DoubleType>())
    , l_returnflag(factory.CreateVarchar())
    , l_linestatus(factory.CreateVarchar())
    , l_shipdate(factory.CreateNumeric<arrow::Date32Type>())
    , l_commitdate(factory.CreateNumeric<arrow::Date32Type>())
    , l_receiptdate(factory.CreateNumeric<arrow::Date32Type>())
    , l_shipinstruct(factory.CreateVarchar())
    , l_shipmode(factory.CreateVarchar())
    , l_comment(factory.CreateVarchar())
{
}

void LineitemColumns::ClearAll()
{
    ResetAll(
        l_orderkey,
        l_partkey,
        l_suppkey,
        l_linenumber,
        l_quantity,
        l_extendedprice,
        l_discount,
        l_tax,
        l_returnflag,
        l_linestatus,
        l_shipdate,
        l_commitdate,
        l_receiptdate,
        l_shipinstruct,
        l_shipmode,
        l_comment);
}

NationColumns::NationColumns(const BuilderFactory & factory)
    : n_nationkey(factory.CreateNumeric<arrow::Int32Type>())
    , n_name(factory.CreateVarchar())
    , n_regionkey(factory.CreateNumeric<arrow::Int32Type>())
    , n_comment(factory.CreateVarchar())
{
}

void NationColumns::ClearAll()
{
    ResetAll(n_nationkey, n_name, n_regionkey, n_comment);
}

RegionColumns::RegionColumns(const BuilderFactory & factory)
    : r_regionkey(factory.CreateNumeric<arrow::Int32Type>())
    , r_name(factory.CreateVarchar())
    , r_comment(factory.CreateVarchar())
{
}

void RegionColumns::ClearAll()
{
    ResetAll(r_regionkey, r_name, r_comment);
}

SupplierColumns::SupplierColumns(const BuilderFactory & factory)
    : s_suppkey(factory.CreateNumeric<arrow::Int32Type>())
    , s_name(factory.CreateVarchar())
    , s_address(factory.CreateVarchar())
    , s_nationkey(factory.CreateNumeric<arrow::Int32Type>())
    , s_phone(factory.CreateVarchar())
    , s_acctbal(factory.CreateNumeric<arrow::DoubleType>())
    , s_comment(factory.CreateVarchar())
{
}

void SupplierColumns::ClearAll()
{
    ResetAll(s_suppkey, s_name, s_address, s_nationkey, s_phone, s_acctbal, s_comment);
}

PartSuppColumns::PartSuppColumns(const BuilderFactory & factory)
    : ps_partkey(factory.CreateNumeric<arrow::Int32Type>())
    , ps_suppkey(factory.CreateNumeric<arrow::Int32Type>())
    , ps_availqty(factory.CreateNumeric<arrow::Int32Type>())
    , ps_supplycost(factory.CreateNumeric<arrow::DoubleType>())
    , ps_comment(factory.CreateVarchar())
{
}

void PartSuppColumns::ClearAll()
{
    ResetAll(ps_partkey, ps_suppkey, ps_availqty, ps_supplycost, ps_comment);
}

PartColumns::PartColumns(const BuilderFactory & factory)
    : p_partkey(factory.CreateNumeric<arrow::Int32Type>())
    , p_name(factory.CreateVarchar())
    , p_mfgr(factory.CreateVarchar())
    , p_brand(factory.CreateVarchar())
    , p_type(factory.CreateVarchar())
    , p_size(factory.CreateNumeric<arrow::Int32Type>())
    , p_container(factory.CreateVarchar())
    , p_retailprice(factory.CreateNumeric<arrow::DoubleType>())
    , p_comment(factory.CreateVarchar())
{
}

void PartColumns::ClearAll()
{
    ResetAll(p_partkey, p_name, p_mfgr, p_brand, p_type, p_size, p_container, p_retailprice, p_comment);
}

const TableMetadata kCustomer = {
    "customer",
    arrow::schema(
        {arrow::field("c_custkey", arrow::int32(), false),
         arrow::field("c_name", arrow::utf8(), false),
         arrow::field("c_address", arrow::utf8(), false),
         arrow::field("c_nationkey", arrow::int32(), false),
         arrow::field("c_phone", arrow::utf8(), false),
         arrow::field("c_acctbal", arrow::float64(), false),
         arrow::field("c_mktsegment", arrow::utf8(), false),
         arrow::field("c_comment", arrow::utf8(), false)}),
    150'000,
};

const TableMetadata kLineitem = {
    "lineitem",
    arrow::schema(
        {arrow::field("l_orderkey", arrow::int32(), false),
         arrow::field("l_partkey", arrow::int32(), false),
         arrow::field("l_suppkey", arrow::int32(), false),
         arrow::field("l_linenumber", arrow::int32(), false),
         arrow::field("l_quantity", arrow::float64(), false),
         arrow::field("l_extendedprice", arrow::float64(), false),
         arrow::field("l_discount", arrow::float64(), false),
         arrow::field("l_tax", arrow::float64(), false),
         arrow::field("l_returnflag", arrow::utf8(), false),
         arrow::field("l_linestatus", arrow::utf8(), false),
         arrow::field("l_shipdate", arrow::date32(), false),
         arrow::field("l_commitdate", arrow::date32(), false),
         arrow::field("l_receiptdate", arrow::date32(), false),
         arrow::field("l_shipinstruct", arrow::utf8(), false),
         arrow::field("l_shipmode", arrow::utf8(), false),
         arrow::field("l_comment", arrow::utf8(), false)}),
    6'000'000,
};

const TableMetadata kNation = {
    "nation",
    arrow::schema(
        {arrow::field("n_nationkey", arrow::int32(), false),
         arrow::field("n_name", arrow::utf8(), false),
         arrow::field("n_regionkey", arrow::int32(), false),
         arrow::field("n_comment", arrow::utf8(), false)}),
    25,
};

const TableMetadata kRegion = {
    "region",
    arrow::schema(
        {arrow::field("r_regionkey", arrow::int32(), false),
         arrow::field("r_name", arrow::utf8(), false),
         arrow::field("r_comment", arrow::utf8(), false)}),
    5,
};

const TableMetadata kOrders = {
    "orders",
    arrow::schema(
        {arrow::field("o_orderkey", arrow::int32(), false),
         arrow::field("o_custkey", arrow::int32(), false),
         arrow::field("o_orderstatus", arrow::utf8(), false),
         arrow::field("o_totalprice", arrow::float64(), false),
         arrow::field("o_orderdate", arrow::date32(), false),
         arrow::field("o_orderpriority", arrow::utf8(), false),
         arrow::field("o_clerk", arrow::utf8(), false),
         arrow::field("o_shippriority", arrow::int32(), false),
         arrow::field("o_comment", arrow::utf8(), false)}),
    1'500'000,
};

const TableMetadata kPart = {
    "part",
    arrow::schema(
        {arrow::field("p_partkey", arrow::int32(), false),
         arrow::field("p_name", arrow::utf8(), false),
         arrow::field("p_mfgr", arrow::utf8(), false),
         arrow::field("p_brand", arrow::utf8(), false),
         arrow::field("p_type", arrow::utf8(), false),
         arrow::field("p_size", arrow::int32(), false),
         arrow::field("p_container", arrow::utf8(), false),
         arrow::field("p_retailprice", arrow::float64(), false),
         arrow::field("p_comment", arrow::utf8(), false)}),
    200'000,
};

const TableMetadata kPartsupp = {
    "partsupp",
    arrow::schema(
        {arrow::field("ps_partkey", arrow::int32(), false),
         arrow::field("ps_suppkey", arrow::int32(), false),
         arrow::field("ps_availqty", arrow::int32(), false),
         arrow::field("ps_supplycost", arrow::float64(), false),
         arrow::field("ps_comment", arrow::utf8(), false)}),
    800'000,
};

const TableMetadata kSupplier = {
    "supplier",
    arrow::schema(
        {arrow::field("s_suppkey", arrow::int32(), false),
         arrow::field("s_name", arrow::utf8(), false),
         arrow::field("s_address", arrow::utf8(), false),
         arrow::field("s_nationkey", arrow::int32(), false),
         arrow::field("s_phone", arrow::utf8(), false),
         arrow::field("s_acctbal", arrow::float64(), false),
         arrow::field("s_comment", arrow::utf8(), false)}),
    10'000,
};
