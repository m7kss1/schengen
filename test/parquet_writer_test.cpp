#include <gtest/gtest.h>

#include "region.h"
#include "parquet_writer.h"
#include "writer.h"

#include <arrow/util/config.h>

#if defined(ARROW_PARQUET)
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#endif

#include <filesystem>
#include <memory>

static const TextPool & text_pool = TextPool::Default();

TEST(ParquetWriterTest, WritesAndReadsBack)
{
#if !defined(ARROW_PARQUET)
    GTEST_SKIP() << "Arrow was built without Parquet support (ARROW_PARQUET is not defined)";
#else
    namespace fs = std::filesystem;
    const fs::path out_dir = fs::path("parquet_writer_test_out");
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir, ec);

    const OutputLocation output{.uri = out_dir.string()};

    WriterOptions options;
    auto parquet_options = std::make_shared<ParquetWriterOptions>();
    parquet_options->max_row_group_rows = 1024;
    options.format = OutputFormat::Parquet;
    options.format_options = parquet_options;

    ParquetTableWriter writer;

    const auto scale = ScaleConfig{.factor = 1.0};
    const auto plan = MakePartitionPlan(kRegion, scale, /*part_num=*/1, /*part_count=*/1);

    arrow::MemoryPool * pool = arrow::default_memory_pool();

    GeneratorContext ctx;
    ctx.scale = scale;
    ctx.partition = plan;
    ctx.text_pool = &text_pool;
    ctx.pool = pool;

    RegionGenerator gen(pool);
    gen.Reset(ctx);

    const PartitionSpec partition{.part_num = 1, .part_count = 1};
    ASSERT_TRUE(writer.OpenPartition(kRegion, output, options, partition, pool).ok());

    TableBatch batch;
    while (gen.NextBatch(/*max_rows=*/1024, &batch)) {
        ASSERT_TRUE(writer.WriteBatch(batch).ok());
    }
    ASSERT_TRUE(writer.ClosePartition().ok());

    const fs::path parquet_path = out_dir / "region" / "region-1.parquet";
    ASSERT_TRUE(fs::exists(parquet_path)) << parquet_path.string();

    auto parquet_reader = parquet::ParquetFileReader::OpenFile(parquet_path.string());
    ASSERT_NE(parquet_reader, nullptr);

    auto arrow_reader_res =
        parquet::arrow::FileReader::Make(pool, std::move(parquet_reader));
    ASSERT_TRUE(arrow_reader_res.ok()) << arrow_reader_res.status().ToString();

    auto arrow_reader = std::move(arrow_reader_res).ValueOrDie();

    std::shared_ptr<arrow::Table> table;
    const auto st = arrow_reader->ReadTable(&table);
    ASSERT_TRUE(st.ok()) << st.ToString();
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(table->num_rows(), 5);
    EXPECT_EQ(table->num_columns(), 3);
    EXPECT_EQ(table->schema()->field(0)->name(), "r_regionkey");
    EXPECT_EQ(table->schema()->field(1)->name(), "r_name");
    EXPECT_EQ(table->schema()->field(2)->name(), "r_comment");
#endif
}
