#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arrow/api.h>
#include <arrow/result.h>

struct TableMetadata
{
    std::string name;
    std::shared_ptr<arrow::Schema> schema;
    std::uint64_t rows_at_sf1;
};

struct TableBatch
{
    const TableMetadata * metadata = nullptr;
    std::uint64_t first_row_id = 0;
    std::uint64_t row_count = 0;
    std::vector<std::shared_ptr<arrow::Array>> columns;
};

template <typename T>
class NumericColumnBuilder
{
public:
    using BuilderType = arrow::NumericBuilder<T>;
    using ValueType = typename T::c_type;

    explicit NumericColumnBuilder(arrow::MemoryPool * pool)
        : builder_(std::make_unique<BuilderType>(pool))
    {
    }

    void Reset() { builder_->Reset(); }

    void Append(ValueType value)
    {
        const auto status = builder_->Append(value);
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
    }

    std::shared_ptr<arrow::Array> Finish()
    {
        std::shared_ptr<arrow::Array> array;
        const auto status = builder_->Finish(&array);
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
        return array;
    }

    BuilderType * builder() { return builder_.get(); }
    const BuilderType * builder() const { return builder_.get(); }

private:
    std::unique_ptr<BuilderType> builder_;
};

class VarcharColumnBuilder
{
public:
    explicit VarcharColumnBuilder(arrow::MemoryPool * pool)
        : builder_(std::make_unique<arrow::StringBuilder>(pool))
    {
    }

    void Reset() { builder_->Reset(); }

    void Append(std::string_view value)
    {
        const auto status = builder_->Append(value);
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
    }

    std::shared_ptr<arrow::Array> Finish()
    {
        std::shared_ptr<arrow::Array> array;
        const auto status = builder_->Finish(&array);
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
        return array;
    }

    arrow::StringBuilder * builder() { return builder_.get(); }
    const arrow::StringBuilder * builder() const { return builder_.get(); }

private:
    std::unique_ptr<arrow::StringBuilder> builder_;
};

class BuilderFactory
{
public:
    explicit BuilderFactory(arrow::MemoryPool * pool)
        : pool_(pool)
    {
    }

    template <typename T>
    NumericColumnBuilder<T> CreateNumeric() const
    {
        return NumericColumnBuilder<T>(pool_);
    }

    VarcharColumnBuilder CreateVarchar() const { return VarcharColumnBuilder(pool_); }

    arrow::MemoryPool * pool() const { return pool_; }

private:
    arrow::MemoryPool * pool_;
};

class ColumnSetBase
{
protected:
    template <typename... Columns>
    static void ResetAll(Columns &... columns)
    {
        (columns.Reset(), ...);
    }
};

class CustomerColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> c_custkey;
    VarcharColumnBuilder c_name;
    VarcharColumnBuilder c_address;
    NumericColumnBuilder<arrow::Int32Type> c_nationkey;
    NumericColumnBuilder<arrow::DoubleType> c_acctbal;

    explicit CustomerColumns(const BuilderFactory & factory)
        : c_custkey(factory.CreateNumeric<arrow::Int32Type>())
        , c_name(factory.CreateVarchar())
        , c_address(factory.CreateVarchar())
        , c_nationkey(factory.CreateNumeric<arrow::Int32Type>())
        , c_acctbal(factory.CreateNumeric<arrow::DoubleType>())
    {
    }

    void ClearAll() { ResetAll(c_custkey, c_name, c_address, c_nationkey, c_acctbal); }
};

class OrdersColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> o_orderkey;
    NumericColumnBuilder<arrow::Int32Type> o_custkey;
    VarcharColumnBuilder o_orderstatus;
    NumericColumnBuilder<arrow::DoubleType> o_totalprice;
    NumericColumnBuilder<arrow::Date32Type> o_orderdate;
    VarcharColumnBuilder o_orderpriority;
    VarcharColumnBuilder o_clerk;
    NumericColumnBuilder<arrow::Int32Type> o_shippriority;
    VarcharColumnBuilder o_comment;

    explicit OrdersColumns(const BuilderFactory & factory)
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

    void ClearAll()
    {
        ResetAll(o_orderkey, o_custkey, o_orderstatus, o_totalprice, o_orderdate, o_orderpriority, o_clerk, o_shippriority, o_comment);
    }
};

class LineitemColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> l_orderkey;
    NumericColumnBuilder<arrow::Int32Type> l_partkey;
    NumericColumnBuilder<arrow::Int32Type> l_suppkey;
    NumericColumnBuilder<arrow::Int32Type> l_linenumber;
    NumericColumnBuilder<arrow::DoubleType> l_quantity;
    NumericColumnBuilder<arrow::DoubleType> l_extendedprice;
    NumericColumnBuilder<arrow::DoubleType> l_discount;
    NumericColumnBuilder<arrow::DoubleType> l_tax;
    VarcharColumnBuilder l_returnflag;
    VarcharColumnBuilder l_linestatus;
    NumericColumnBuilder<arrow::Date32Type> l_shipdate;
    NumericColumnBuilder<arrow::Date32Type> l_commitdate;
    NumericColumnBuilder<arrow::Date32Type> l_receiptdate;
    VarcharColumnBuilder l_shipinstruct;
    VarcharColumnBuilder l_shipmode;
    VarcharColumnBuilder l_comment;

    explicit LineitemColumns(const BuilderFactory & factory)
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

    void ClearAll()
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
};

class NationColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> n_nationkey;
    VarcharColumnBuilder n_name;
    NumericColumnBuilder<arrow::Int32Type> n_regionkey;
    VarcharColumnBuilder n_comment;

    explicit NationColumns(const BuilderFactory & factory)
        : n_nationkey(factory.CreateNumeric<arrow::Int32Type>())
        , n_name(factory.CreateVarchar())
        , n_regionkey(factory.CreateNumeric<arrow::Int32Type>())
        , n_comment(factory.CreateVarchar())
    {
    }

    void ClearAll()
    {
        ResetAll(
            n_nationkey,
            n_name,
            n_regionkey,
            n_comment);
    }
};

class RegionColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> r_regionkey;
    VarcharColumnBuilder r_name;
    VarcharColumnBuilder r_comment;

    explicit RegionColumns(const BuilderFactory & factory)
        : r_regionkey(factory.CreateNumeric<arrow::Int32Type>())
        , r_name(factory.CreateVarchar())
        , r_comment(factory.CreateVarchar())
    {
    }

    void ClearAll()
    {
        ResetAll(
            r_regionkey,
            r_name,
            r_comment);
    }
};

inline const TableMetadata kCustomer = {
    "customer",
    arrow::schema(
        {arrow::field("c_custkey", arrow::int32(), false),
         arrow::field("c_name", arrow::utf8(), false),
         arrow::field("c_address", arrow::utf8(), false),
         arrow::field("c_nationkey", arrow::int32(), false),
         arrow::field("c_acctbal", arrow::float64(), false)}),
    150'000,
};

inline const TableMetadata kLineitem = {
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

inline const TableMetadata kNation = {
    "nation",
    arrow::schema(
        {arrow::field("n_nationkey", arrow::int32(), false),
         arrow::field("n_name", arrow::utf8(), false),
         arrow::field("n_regionkey", arrow::int32(), false),
         arrow::field("n_comment", arrow::utf8(), false)}),
    25,
};

inline const TableMetadata kRegion = {
    "region",
    arrow::schema(
        {arrow::field("r_regionkey", arrow::int32(), false),
         arrow::field("r_name", arrow::utf8(), false),
         arrow::field("r_comment", arrow::utf8(), false)}),
    5,
};

inline const TableMetadata kOrders = {
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

inline const TableMetadata kPart = {
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

inline const TableMetadata kPartsupp = {
    "partsupp",
    arrow::schema(
        {arrow::field("ps_partkey", arrow::int32(), false),
         arrow::field("ps_suppkey", arrow::int32(), false),
         arrow::field("ps_availqty", arrow::int32(), false),
         arrow::field("ps_supplycost", arrow::float64(), false),
         arrow::field("ps_comment", arrow::utf8(), false)}),
    800'000,
};

inline const TableMetadata kSupplier = {
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
