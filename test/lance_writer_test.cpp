#include <gtest/gtest.h>

#include "register_tables.h"
#include "region.h"
#include "lance_bridge.h"
#include "lance_writer.h"
#include "write_scheduler.h"
#include "writer.h"
#include "yaclib/exe/inline.hpp"

#include <filesystem>
#include <memory>
#include <string>

static const TextPool & lance_text_pool = TextPool::Default();

namespace
{
GeneratorContext MakeRegionContext(arrow::MemoryPool * pool, std::int32_t part_num, std::int32_t part_count)
{
    const auto scale = ScaleConfig{.factor = 1.0};
    GeneratorContext ctx;
    ctx.scale = scale;
    ctx.partition = MakePartitionPlan(kRegion, scale, part_num, part_count);
    ctx.text_pool = &lance_text_pool;
    ctx.pool = pool;
    return ctx;
}

std::shared_ptr<LanceWriterOptions> MakeDefaultLanceOptions()
{
    return std::make_shared<LanceWriterOptions>();
}

GenerationContext MakeGenerationContext(arrow::MemoryPool * pool, const ScaleConfig & scale, std::uint64_t batch_rows)
{
    RegisterTables();

    GenerationContext ctx;
    ctx.registry = &TableRegistry::Instance();
    ctx.text_pool = &lance_text_pool;
    ctx.scale = &scale;
    ctx.pool = pool;
    ctx.batch_rows = batch_rows;
    return ctx;
}
} // namespace

TEST(LanceWriterTest, WritesAndReadsBack)
{
#if !defined(ENABLE_LANCE)
    GTEST_SKIP() << "Lance is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("lance_writer_test_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    WriterOptions options;
    options.format = OutputFormat::Lance;
    options.format_options = MakeDefaultLanceOptions();

    LanceOrderedWriter writer;
    arrow::MemoryPool * pool = arrow::default_memory_pool();
    const OutputLocation output{.uri = out_dir.string()};

    RegionGenerator gen(pool);
    auto gen_ctx = MakeRegionContext(pool, /*part_num=*/1, /*part_count=*/1);
    gen.Reset(gen_ctx);

    ASSERT_TRUE(writer.OpenTable(kRegion, output, options, pool).ok());
    ASSERT_TRUE(writer.BeginInputPartition(PartitionSpec{.part_num = 1, .part_count = 1}).ok());

    TableBatch batch;
    while (gen.NextBatch(/*max_rows=*/1024, &batch))
    {
        ASSERT_TRUE(writer.WriteBatch(batch).ok());
    }

    ASSERT_TRUE(writer.EndInputPartition().ok());
    ASSERT_TRUE(writer.CloseTable().ok());

    const fs::path dataset_path = out_dir / "region" / "region-1.lance";
    ASSERT_TRUE(fs::exists(dataset_path)) << dataset_path.string();

    LanceDatasetInfoC info{.row_count = 0, .field_names_csv = nullptr};
    ASSERT_EQ(lance_dataset_inspect(dataset_path.c_str(), &info), 0) << lance_bridge_last_error();
    ASSERT_NE(info.field_names_csv, nullptr);
    EXPECT_EQ(info.row_count, 5);
    EXPECT_EQ(std::string(info.field_names_csv), "r_regionkey,r_name,r_comment");
    lance_dataset_info_destroy(&info);
#endif
}

TEST(LanceWriterTest, FailsIfDatasetAlreadyExists)
{
#if !defined(ENABLE_LANCE)
    GTEST_SKIP() << "Lance is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("lance_writer_exists_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    WriterOptions options;
    options.format = OutputFormat::Lance;
    options.format_options = MakeDefaultLanceOptions();

    arrow::MemoryPool * pool = arrow::default_memory_pool();
    const OutputLocation output{.uri = out_dir.string()};

    RegionGenerator gen(pool);
    gen.Reset(MakeRegionContext(pool, /*part_num=*/1, /*part_count=*/1));

    LanceOrderedWriter writer;
    ASSERT_TRUE(writer.OpenTable(kRegion, output, options, pool).ok());
    ASSERT_TRUE(writer.BeginInputPartition(PartitionSpec{.part_num = 1, .part_count = 1}).ok());

    TableBatch batch;
    while (gen.NextBatch(/*max_rows=*/1024, &batch))
    {
        ASSERT_TRUE(writer.WriteBatch(batch).ok());
    }
    ASSERT_TRUE(writer.EndInputPartition().ok());
    ASSERT_TRUE(writer.CloseTable().ok());

    LanceOrderedWriter second_writer;
    const auto status = second_writer.OpenTable(kRegion, output, options, pool);
    ASSERT_FALSE(status.ok());
    EXPECT_NE(status.ToString().find("exist"), std::string::npos);
#endif
}

TEST(LanceFormatDriverTest, PrefersSingleFileOrdered)
{
#if !defined(ENABLE_LANCE)
    GTEST_SKIP() << "Lance is not enabled";
#else
    auto driver_result = ResolveFormatDriver("lance");
    ASSERT_TRUE(driver_result.ok()) << driver_result.status().ToString();
    const IFormatDriver * driver = driver_result.ValueOrDie();

    EXPECT_EQ(driver->PreferredStrategy(), WriteStrategy::SingleFileOrdered);
    EXPECT_TRUE(driver->SupportsStrategy(WriteStrategy::SingleFileOrdered));
    EXPECT_FALSE(driver->SupportsStrategy(WriteStrategy::ParallelPartitionFiles));
    EXPECT_EQ(driver->OrderedExecutionModel(), OrderedWriteExecutionModel::ForeignStreaming);

    WriteSchedulerOptions auto_options;
    auto auto_strategy = ResolveSelectedWriteStrategy(auto_options, *driver);
    ASSERT_TRUE(auto_strategy.ok()) << auto_strategy.status().ToString();
    EXPECT_EQ(auto_strategy.ValueOrDie(), WriteStrategy::SingleFileOrdered);

    WriteSchedulerOptions invalid_options;
    invalid_options.strategy = WriteStrategy::ParallelPartitionFiles;
    const auto invalid_strategy = ResolveSelectedWriteStrategy(invalid_options, *driver);
    ASSERT_FALSE(invalid_strategy.ok());
    EXPECT_NE(invalid_strategy.status().ToString().find("lance"), std::string::npos);
#endif
}

TEST(LanceWriteSchedulerTest, SingleFileOrderedStreamsPartitionsDirectly)
{
#if !defined(ENABLE_LANCE)
    GTEST_SKIP() << "Lance is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("lance_scheduler_ordered_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    auto driver_result = ResolveFormatDriver("lance");
    ASSERT_TRUE(driver_result.ok()) << driver_result.status().ToString();
    const IFormatDriver * driver = driver_result.ValueOrDie();

    auto lance_options = MakeDefaultLanceOptions();
    lance_options->target_partition_rows = 2;

    WriterOptions writer_options;
    writer_options.format = OutputFormat::Lance;
    writer_options.format_options = lance_options;

    WriteSchedulerOptions scheduler_options;
    scheduler_options.strategy = WriteStrategy::SingleFileOrdered;
    scheduler_options.queue_capacity = 2;

    arrow::MemoryPool * pool = arrow::default_memory_pool();
    const ScaleConfig scale{.factor = 1.0};
    auto generation_ctx = MakeGenerationContext(pool, scale, /*batch_rows=*/2);
    const OutputLocation output{.uri = out_dir.string()};
    auto & part_executor = yaclib::MakeInline();

    ASSERT_EQ(
        GenerateTableWithStrategy(generation_ctx, kRegion, output, writer_options, *driver, scheduler_options, part_executor),
        0);

    const fs::path dataset_path = out_dir / "region" / "region-1.lance";
    ASSERT_TRUE(fs::exists(dataset_path)) << dataset_path.string();

    LanceDatasetInfoC info{.row_count = 0, .field_names_csv = nullptr};
    ASSERT_EQ(lance_dataset_inspect(dataset_path.c_str(), &info), 0) << lance_bridge_last_error();
    ASSERT_NE(info.field_names_csv, nullptr);
    EXPECT_EQ(info.row_count, 5);
    EXPECT_EQ(std::string(info.field_names_csv), "r_regionkey,r_name,r_comment");
    lance_dataset_info_destroy(&info);
#endif
}
