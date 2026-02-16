#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arrow/api.h>

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
        if (!status.ok())
        {
            throw std::runtime_error(status.ToString());
        }
    }

    std::shared_ptr<arrow::Array> Finish()
    {
        std::shared_ptr<arrow::Array> array;
        const auto status = builder_->Finish(&array);
        if (!status.ok())
        {
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
    explicit VarcharColumnBuilder(arrow::MemoryPool * pool);
    void Reset();
    void Append(std::string_view value);
    std::shared_ptr<arrow::Array> Finish();
    arrow::StringBuilder * builder();
    const arrow::StringBuilder * builder() const;

private:
    std::unique_ptr<arrow::StringBuilder> builder_;
};

class BuilderFactory
{
public:
    explicit BuilderFactory(arrow::MemoryPool * pool);

    template <typename T>
    NumericColumnBuilder<T> CreateNumeric() const
    {
        return NumericColumnBuilder<T>(pool_);
    }

    VarcharColumnBuilder CreateVarchar() const;
    arrow::MemoryPool * pool() const;

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
    VarcharColumnBuilder c_phone;
    NumericColumnBuilder<arrow::DoubleType> c_acctbal;
    VarcharColumnBuilder c_mktsegment;
    VarcharColumnBuilder c_comment;

    explicit CustomerColumns(const BuilderFactory & factory);
    void ClearAll();
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

    explicit OrdersColumns(const BuilderFactory & factory);
    void ClearAll();
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

    explicit LineitemColumns(const BuilderFactory & factory);
    void ClearAll();
};

class NationColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> n_nationkey;
    VarcharColumnBuilder n_name;
    NumericColumnBuilder<arrow::Int32Type> n_regionkey;
    VarcharColumnBuilder n_comment;

    explicit NationColumns(const BuilderFactory & factory);
    void ClearAll();
};

class RegionColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> r_regionkey;
    VarcharColumnBuilder r_name;
    VarcharColumnBuilder r_comment;

    explicit RegionColumns(const BuilderFactory & factory);
    void ClearAll();
};

class SupplierColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> s_suppkey;
    VarcharColumnBuilder s_name;
    VarcharColumnBuilder s_address;
    NumericColumnBuilder<arrow::Int32Type> s_nationkey;
    VarcharColumnBuilder s_phone;
    NumericColumnBuilder<arrow::DoubleType> s_acctbal;
    VarcharColumnBuilder s_comment;

    explicit SupplierColumns(const BuilderFactory & factory);
    void ClearAll();
};

class PartSuppColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> ps_partkey;
    NumericColumnBuilder<arrow::Int32Type> ps_suppkey;
    NumericColumnBuilder<arrow::Int32Type> ps_availqty;
    NumericColumnBuilder<arrow::DoubleType> ps_supplycost;
    VarcharColumnBuilder ps_comment;

    explicit PartSuppColumns(const BuilderFactory & factory);
    void ClearAll();
};

class PartColumns : public ColumnSetBase
{
public:
    NumericColumnBuilder<arrow::Int32Type> p_partkey;
    VarcharColumnBuilder p_name;
    VarcharColumnBuilder p_mfgr;
    VarcharColumnBuilder p_brand;
    VarcharColumnBuilder p_type;
    NumericColumnBuilder<arrow::Int32Type> p_size;
    VarcharColumnBuilder p_container;
    NumericColumnBuilder<arrow::DoubleType> p_retailprice;
    VarcharColumnBuilder p_comment;

    explicit PartColumns(const BuilderFactory & factory);
    void ClearAll();
};

extern const TableMetadata kCustomer;
extern const TableMetadata kLineitem;
extern const TableMetadata kNation;
extern const TableMetadata kRegion;
extern const TableMetadata kOrders;
extern const TableMetadata kPart;
extern const TableMetadata kPartsupp;
extern const TableMetadata kSupplier;
