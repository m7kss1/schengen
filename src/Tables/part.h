#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

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
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool);
    bool NextValue(PartRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static std::string FormatManufacturer(std::int32_t mfgr_key);
    static std::string FormatBrand(std::int32_t brand_key);
    static std::int32_t CalculatePartPrice(std::int32_t part_key);

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
    explicit PartGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    PartColumns columns_;
    PartRowIterator row_iter_;
};
