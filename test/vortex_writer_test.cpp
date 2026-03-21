#include <gtest/gtest.h>

#include "region.h"
#include "vortex_bridge.h"
#include "vortex_writer.h"
#include "write_scheduler.h"
#include "writer.h"

#include <boost/program_options.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

static const TextPool & vortex_text_pool = TextPool::Default();

namespace po = boost::program_options;

namespace
{
GeneratorContext MakeRegionContext(arrow::MemoryPool * pool, std::int32_t part_num, std::int32_t part_count)
{
    const auto scale = ScaleConfig{.factor = 1.0};
    GeneratorContext ctx;
    ctx.scale = scale;
    ctx.partition = MakePartitionPlan(kRegion, scale, part_num, part_count);
    ctx.text_pool = &vortex_text_pool;
    ctx.pool = pool;
    return ctx;
}

std::shared_ptr<VortexWriterOptions> MakeDefaultVortexOptions()
{
    return std::make_shared<VortexWriterOptions>();
}

arrow::Result<WriterOptions> ParseVortexWriterOptions(const IFormatDriver & driver, const std::vector<std::string> & args)
{
    po::options_description desc("test");
    RegisterFormatCliOptions(desc);

    po::variables_map vm;
    try
    {
        po::store(po::command_line_parser(args).options(desc).run(), vm);
        po::notify(vm);
    }
    catch (const std::exception & ex)
    {
        return arrow::Status::Invalid(ex.what());
    }
    return driver.BuildWriterOptions(vm);
}
} // namespace

TEST(VortexWriterTest, WritesAndReadsBack)
{
#if !defined(ENABLE_VORTEX)
    GTEST_SKIP() << "Vortex is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("vortex_writer_test_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    WriterOptions options;
    options.format = OutputFormat::Vortex;
    options.format_options = MakeDefaultVortexOptions();

    VortexOrderedWriter writer;
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

    const fs::path file_path = out_dir / "region" / "region-1.vortex";
    ASSERT_TRUE(fs::exists(file_path)) << file_path.string();

    VortexFileInfoC info{.row_count = 0, .field_names_csv = nullptr};
    ASSERT_EQ(vortex_file_inspect(file_path.c_str(), &info), 0) << vortex_bridge_last_error();
    ASSERT_NE(info.field_names_csv, nullptr);
    EXPECT_EQ(info.row_count, 5);
    EXPECT_EQ(std::string(info.field_names_csv), "r_regionkey,r_name,r_comment");
    vortex_file_info_destroy(&info);
#endif
}

TEST(VortexWriterTest, CreatesValidEmptyFile)
{
#if !defined(ENABLE_VORTEX)
    GTEST_SKIP() << "Vortex is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("vortex_writer_empty_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    WriterOptions options;
    options.format = OutputFormat::Vortex;
    options.format_options = MakeDefaultVortexOptions();

    VortexOrderedWriter writer;
    arrow::MemoryPool * pool = arrow::default_memory_pool();
    const OutputLocation output{.uri = out_dir.string()};

    ASSERT_TRUE(writer.OpenTable(kRegion, output, options, pool).ok());
    ASSERT_TRUE(writer.BeginInputPartition(PartitionSpec{.part_num = 1, .part_count = 1}).ok());
    ASSERT_TRUE(writer.EndInputPartition().ok());
    ASSERT_TRUE(writer.CloseTable().ok());

    const fs::path file_path = out_dir / "region" / "region-1.vortex";
    ASSERT_TRUE(fs::exists(file_path)) << file_path.string();

    VortexFileInfoC info{.row_count = 0, .field_names_csv = nullptr};
    ASSERT_EQ(vortex_file_inspect(file_path.c_str(), &info), 0) << vortex_bridge_last_error();
    ASSERT_NE(info.field_names_csv, nullptr);
    EXPECT_EQ(info.row_count, 0);
    EXPECT_EQ(std::string(info.field_names_csv), "r_regionkey,r_name,r_comment");
    vortex_file_info_destroy(&info);
#endif
}

TEST(VortexWriterTest, FailsIfFileAlreadyExists)
{
#if !defined(ENABLE_VORTEX)
    GTEST_SKIP() << "Vortex is not enabled";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("vortex_writer_exists_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    WriterOptions options;
    options.format = OutputFormat::Vortex;
    options.format_options = MakeDefaultVortexOptions();

    arrow::MemoryPool * pool = arrow::default_memory_pool();
    const OutputLocation output{.uri = out_dir.string()};

    RegionGenerator gen(pool);
    gen.Reset(MakeRegionContext(pool, /*part_num=*/1, /*part_count=*/1));

    VortexOrderedWriter writer;
    ASSERT_TRUE(writer.OpenTable(kRegion, output, options, pool).ok());
    ASSERT_TRUE(writer.BeginInputPartition(PartitionSpec{.part_num = 1, .part_count = 1}).ok());

    TableBatch batch;
    while (gen.NextBatch(/*max_rows=*/1024, &batch))
    {
        ASSERT_TRUE(writer.WriteBatch(batch).ok());
    }
    ASSERT_TRUE(writer.EndInputPartition().ok());
    ASSERT_TRUE(writer.CloseTable().ok());

    VortexOrderedWriter second_writer;
    const auto status = second_writer.OpenTable(kRegion, output, options, pool);
    ASSERT_FALSE(status.ok());
    EXPECT_NE(status.ToString().find("exist"), std::string::npos);
#endif
}

TEST(VortexFormatDriverTest, PrefersSingleFileOrdered)
{
#if !defined(ENABLE_VORTEX)
    GTEST_SKIP() << "Vortex is not enabled";
#else
    auto driver_result = ResolveFormatDriver("vortex");
    ASSERT_TRUE(driver_result.ok()) << driver_result.status().ToString();
    const IFormatDriver * driver = driver_result.ValueOrDie();

    EXPECT_EQ(driver->PreferredStrategy(), WriteStrategy::SingleFileOrdered);
    EXPECT_TRUE(driver->SupportsStrategy(WriteStrategy::SingleFileOrdered));
    EXPECT_FALSE(driver->SupportsStrategy(WriteStrategy::ParallelPartitionFiles));

    WriteSchedulerOptions auto_options;
    auto auto_strategy = ResolveSelectedWriteStrategy(auto_options, *driver);
    ASSERT_TRUE(auto_strategy.ok()) << auto_strategy.status().ToString();
    EXPECT_EQ(auto_strategy.ValueOrDie(), WriteStrategy::SingleFileOrdered);

    WriteSchedulerOptions invalid_options;
    invalid_options.strategy = WriteStrategy::ParallelPartitionFiles;
    const auto invalid_strategy = ResolveSelectedWriteStrategy(invalid_options, *driver);
    ASSERT_FALSE(invalid_strategy.ok());
    EXPECT_NE(invalid_strategy.status().ToString().find("vortex"), std::string::npos);
#endif
}

TEST(VortexFormatDriverTest, SupportsPrimaryAndLegacyPartitionOptions)
{
#if !defined(ENABLE_VORTEX)
    GTEST_SKIP() << "Vortex is not enabled";
#else
    auto driver_result = ResolveFormatDriver("vortex");
    ASSERT_TRUE(driver_result.ok()) << driver_result.status().ToString();
    const IFormatDriver * driver = driver_result.ValueOrDie();

    auto primary_options_result = ParseVortexWriterOptions(*driver, {"--vortex-target-partition-rows", "17"});
    ASSERT_TRUE(primary_options_result.ok()) << primary_options_result.status().ToString();
    auto primary_options = primary_options_result.ValueOrDie();
    EXPECT_EQ(VortexOrderedWriter::ResolveOptions(primary_options).target_partition_rows, 17);

    auto legacy_options_result = ParseVortexWriterOptions(*driver, {"--vortex-max-partition-rows", "19"});
    ASSERT_TRUE(legacy_options_result.ok()) << legacy_options_result.status().ToString();
    auto legacy_options = legacy_options_result.ValueOrDie();
    EXPECT_EQ(VortexOrderedWriter::ResolveOptions(legacy_options).target_partition_rows, 19);

    auto conflict_result = ParseVortexWriterOptions(
        *driver, {"--vortex-target-partition-rows", "17", "--vortex-max-partition-rows", "19"});
    ASSERT_FALSE(conflict_result.ok());
    EXPECT_NE(conflict_result.status().ToString().find("must match"), std::string::npos);
#endif
}
