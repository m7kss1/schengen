#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "distribution.h"
#include "generator.h"
#include "rands.h"
#include "table.h"

struct PartRow
{
    std::int32_t p_partkey = 0;
    StringSequenceInstance p_name_tokens;
    std::string_view p_name;
    std::string_view p_mfgr;
    std::string_view p_brand;
    std::string_view p_type;
    std::int32_t p_size = 0;
    std::string_view p_container;
    std::int32_t p_retailprice_cents = 0;
    double p_retailprice = 0.0;
    std::string_view p_comment;
};

class PartRowIterator
{
public:
    void Reset(std::uint64_t start_row,
               std::uint64_t end_row,
               const TextPool & text_pool)
    {
        next_row_ = start_row;
        end_row_ = end_row;
        if (next_row_ > end_row_) {
            next_row_ = end_row_;
        }

        name_random_ = RandomStringSequence(
            kNameSeed,
            kNameWords,
            kColorsDist);
        manufacturer_random_ = RandomBoundedInt(
            kManufacturerSeed,
            kManufacturerMin,
            kManufacturerMax);
        brand_random_ = RandomBoundedInt(
            kBrandSeed,
            kBrandMin,
            kBrandMax);
        type_random_ = RandomString(
            kTypeSeed,
            kPartTypesDist);
        size_random_ = RandomBoundedInt(
            kSizeSeed,
            kSizeMin,
            kSizeMax);
        container_random_ = RandomString(
            kContainerSeed,
            kPartContainersDist);
        comment_random_ = RandomText(
            kCommentSeed,
            text_pool,
            static_cast<double>(kCommentAverageLength));

        if (next_row_ > 0) {
            const auto rows = static_cast<std::int64_t>(next_row_);
            name_random_.AdvanceRows(rows);
            manufacturer_random_.AdvanceRows(rows);
            brand_random_.AdvanceRows(rows);
            type_random_.AdvanceRows(rows);
            size_random_.AdvanceRows(rows);
            container_random_.AdvanceRows(rows);
            comment_random_.AdvanceRows(rows);
        }
    }

    bool NextValue(PartRow * out)
    {
        if (next_row_ >= end_row_) {
            return false;
        }

        const std::int32_t part_key = static_cast<std::int32_t>(next_row_ + 1);

        if (out != nullptr) {
            const StringSequenceInstance name_tokens = name_random_.NextValue();
            name_tokens.ToString(name_buffer_);

            const std::int32_t mfgr_key = manufacturer_random_.NextValue();
            const std::int32_t brand_key = mfgr_key * 10 + brand_random_.NextValue();

            mfgr_buffer_ = FormatManufacturer(mfgr_key);
            brand_buffer_ = FormatBrand(brand_key);

            const std::string_view type = type_random_.NextValue();
            const std::int32_t size = size_random_.NextValue();
            const std::string_view container = container_random_.NextValue();
            const std::string_view comment = comment_random_.NextValue();

            const std::int32_t price_cents = CalculatePartPrice(part_key);

            out->p_partkey = part_key;
            out->p_name_tokens = name_tokens;
            out->p_name = std::string_view(name_buffer_);
            out->p_mfgr = std::string_view(mfgr_buffer_);
            out->p_brand = std::string_view(brand_buffer_);
            out->p_type = type;
            out->p_size = size;
            out->p_container = container;
            out->p_retailprice_cents = price_cents;
            out->p_retailprice = static_cast<double>(price_cents) / 100.0;
            out->p_comment = comment;
        }

        name_random_.RowFinished();
        manufacturer_random_.RowFinished();
        brand_random_.RowFinished();
        type_random_.RowFinished();
        size_random_.RowFinished();
        container_random_.RowFinished();
        comment_random_.RowFinished();

        ++next_row_;
        return true;
    }

    bool Done() const { return next_row_ >= end_row_; }
    std::uint64_t NextRowId() const { return next_row_; }

private:
    static std::string FormatManufacturer(std::int32_t mfgr_key)
    {
        char buffer[32];
        const int n = std::snprintf(buffer, sizeof(buffer), "Manufacturer#%d", mfgr_key);
        return std::string(buffer, buffer + (n > 0 ? n : 0));
    }

    static std::string FormatBrand(std::int32_t brand_key)
    {
        char buffer[32];
        const int n = std::snprintf(buffer, sizeof(buffer), "Brand#%d", brand_key);
        return std::string(buffer, buffer + (n > 0 ? n : 0));
    }

    static std::int32_t CalculatePartPrice(std::int32_t part_key)
    {
        std::int32_t price = 90000;
        price += (part_key / 10) % 20001;
        price += (part_key % 1000) * 100;
        return price;
    }

    static constexpr std::int32_t kNameWords = 5;
    static constexpr std::int32_t kManufacturerMin = 1;
    static constexpr std::int32_t kManufacturerMax = 5;
    static constexpr std::int32_t kBrandMin = 1;
    static constexpr std::int32_t kBrandMax = 5;
    static constexpr std::int32_t kSizeMin = 1;
    static constexpr std::int32_t kSizeMax = 50;
    static constexpr std::int32_t kCommentAverageLength = 14;

    static constexpr std::int64_t kNameSeed = 709314158;
    static constexpr std::int64_t kManufacturerSeed = 1;
    static constexpr std::int64_t kBrandSeed = 46831694;
    static constexpr std::int64_t kTypeSeed = 1841581359;
    static constexpr std::int64_t kSizeSeed = 1193163244;
    static constexpr std::int64_t kContainerSeed = 727633698;
    static constexpr std::int64_t kCommentSeed = 804159733;

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;

    RandomStringSequence name_random_{};
    RandomBoundedInt manufacturer_random_{};
    RandomBoundedInt brand_random_{};
    RandomString type_random_{};
    RandomBoundedInt size_random_{};
    RandomString container_random_{};
    RandomText comment_random_{};

    std::string name_buffer_;
    std::string mfgr_buffer_;
    std::string brand_buffer_;
};

class PartGenerator final : public ITableGenerator
{
public:
    explicit PartGenerator(arrow::MemoryPool * pool)
        : factory_(pool)
        , columns_(factory_)
    {
    }

    const TableMetadata & GetTableMetadata() const override { return kPart; }

    void Reset(const GeneratorContext & ctx) override
    {
        ctx_ = ctx;
        columns_.ClearAll();

        const TextPool & text_pool = ctx.text_pool != nullptr
            ? *ctx.text_pool
            : TextPool::Default();

        row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, text_pool);
    }

    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override
    {
        if (max_rows == 0 || row_iter_.Done()) {
            return false;
        }

        columns_.ClearAll();

        const std::uint64_t batch_start = row_iter_.NextRowId();
        std::uint64_t batch_count = 0;

        PartRow row;
        while (batch_count < max_rows && row_iter_.NextValue(&row)) {
            columns_.p_partkey.Append(row.p_partkey);
            columns_.p_name.Append(row.p_name);
            columns_.p_mfgr.Append(row.p_mfgr);
            columns_.p_brand.Append(row.p_brand);
            columns_.p_type.Append(row.p_type);
            columns_.p_size.Append(row.p_size);
            columns_.p_container.Append(row.p_container);
            columns_.p_retailprice.Append(row.p_retailprice);
            columns_.p_comment.Append(row.p_comment);
            ++batch_count;
        }

        if (out != nullptr) {
            out->metadata = &kPart;
            out->first_row_id = batch_start;
            out->row_count = batch_count;
            out->columns = {
                columns_.p_partkey.Finish(),
                columns_.p_name.Finish(),
                columns_.p_mfgr.Finish(),
                columns_.p_brand.Finish(),
                columns_.p_type.Finish(),
                columns_.p_size.Finish(),
                columns_.p_container.Finish(),
                columns_.p_retailprice.Finish(),
                columns_.p_comment.Finish(),
            };
        }

        return true;
    }

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    PartColumns columns_;
    PartRowIterator row_iter_;
};
