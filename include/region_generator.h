#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "generator.h"
#include "table.h"

class RegionGenerator final : public ITableGenerator
{
public:
    explicit RegionGenerator(arrow::MemoryPool * pool)
        : factory_(pool)
        , columns_(factory_)
    {
    }

    const TableMetadata & GetTableMetadata() const override { return kRegion; }

    void Reset(const GeneratorContext & ctx) override
    {
        ctx_ = ctx;
        columns_.ClearAll();
        next_row_ = ctx.partition.range.start_row;
        end_row_ = std::min<std::uint64_t>(ctx.partition.range.end_row, kRegionCount);
        
        if (next_row_ > end_row_) {
            next_row_ = end_row_;
        }
    }

    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override
    {
        if (max_rows == 0 || next_row_ >= end_row_) {
            return false;
        }

        columns_.ClearAll();

        const std::uint64_t batch_start = next_row_;
        const std::uint64_t remaining = end_row_ - next_row_;
        const std::uint64_t batch_count = std::min(max_rows, remaining);

        for (std::size_t i = 0; i < batch_count; ++i) {
            const auto row_id = batch_start + i;

            columns_.r_regionkey.Append(static_cast<std::int32_t>(row_id));
            columns_.r_name.Append(kRegionNames[row_id]);
            /// TODO: FIXME replace with RandomText when TextPool is done
            columns_.r_comment.Append(kEmptyComment);
        }

        next_row_ += batch_count;

        if (out != nullptr) {
            out->metadata = &kRegion;
            out->first_row_id = batch_start;
            out->row_count = batch_count;
            out->columns = {
                columns_.r_regionkey.Finish(),
                columns_.r_name.Finish(),
                columns_.r_comment.Finish(),
            };
        }

        return true;
    }

private:
    static constexpr std::array<const char *, 5> kRegionNames = {
        "AFRICA",
        "AMERICA",
        "ASIA",
        "EUROPE",
        "MIDDLE EAST",
    };
    static constexpr const char * kEmptyComment = "";
    static constexpr std::uint64_t kRegionCount = kRegionNames.size();
    static const/*expr :) */ TableMetadata kRegion = {
        "region",
        arrow::schema(
            {arrow::field("r_regionkey", arrow::int32(), false),
            arrow::field("r_name", arrow::utf8(), false),
            arrow::field("r_comment", arrow::utf8(), false)}),
        5,
    };

    GeneratorContext ctx_{};
    BuilderFactory factory_;
    RegionColumns columns_;
    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;
};
