#include "Storage/write_scheduler.h"

#include "Storage/orc_writer.h"
#include "Storage/parquet_writer.h"
#include <boost/program_options.hpp>
#include <arrow/io/buffered.h>

#include "yaclib/async/future.hpp"
#include "yaclib/async/run.hpp"
#include "yaclib_std/atomic"
#include "yaclib_std/condition_variable"
#include "yaclib_std/mutex"

#include <algorithm>
#include <cctype>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace po = boost::program_options;

namespace
{
std::string ToLower(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::uint32_t ResolveWorkerCount(std::uint32_t configured, std::int32_t part_count)
{
    const auto max_workers = static_cast<std::uint32_t>(std::max<std::int32_t>(1, part_count));
    if (configured == 0)
    {
        return max_workers;
    }
    return std::min(configured, max_workers);
}

namespace detail
{
template <class T>
class IQueue
{
public:
    virtual ~IQueue() = default;
    virtual bool Push(T value) = 0;
    virtual bool Pop(T * value) = 0;
    virtual void Close() = 0;
};

class PartitionRangeQueue final : public IQueue<std::int32_t>
{
public:
    explicit PartitionRangeQueue(std::int32_t part_count)
        : part_count_(part_count)
    {
    }

    bool Push(std::int32_t value) override
    {
        (void)value;
        return false;
    }

    bool Pop(std::int32_t * value) override
    {
        if (closed_.load(std::memory_order_acquire))
        {
            return false;
        }

        const auto part = next_.fetch_add(1, std::memory_order_acq_rel);
        if (part > part_count_)
        {
            return false;
        }

        *value = part;
        return true;
    }

    void Close() override
    {
        closed_.store(true, std::memory_order_release);
    }

private:
    yaclib_std::atomic<std::int32_t> next_{1};
    std::int32_t part_count_ = 0;
    yaclib_std::atomic<bool> closed_{false};
};

template <class T>
class BoundedMpscQueue : public IQueue<T>
{
public:
    explicit BoundedMpscQueue(std::size_t capacity)
        : capacity_(std::max<std::size_t>(1, capacity))
        , ring_(capacity_)
    {
        for (std::size_t i = 0; i < capacity_; ++i)
        {
            ring_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool Push(T value) override
    {
        while (true)
        {
            if (closed_.load(std::memory_order_acquire))
            {
                return false;
            }

            std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            Slot & slot = ring_[Index(pos)];
            const auto seq = slot.sequence.load(std::memory_order_acquire);
            const auto diff = SequenceDiff(seq, pos);

            if (diff == 0)
            {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    slot.value = std::move(value);
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    not_empty_cv_.notify_one();
                    return true;
                }
                continue;
            }

            if (diff < 0)
            {
                std::unique_lock<yaclib_std::mutex> lock(not_full_mutex_);
                not_full_cv_.wait(lock, [this]() {
                    if (closed_.load(std::memory_order_acquire))
                    {
                        return true;
                    }
                    return CanProducerReserveSlot();
                });
            }
        }
    }

    bool Pop(T * value) override
    {
        while (true)
        {
            const std::size_t pos = dequeue_pos_;
            Slot & slot = ring_[Index(pos)];
            const auto seq = slot.sequence.load(std::memory_order_acquire);
            const auto diff = SequenceDiff(seq, pos + 1);

            if (diff == 0)
            {
                *value = std::move(slot.value);
                slot.sequence.store(pos + capacity_, std::memory_order_release);
                dequeue_pos_ = pos + 1;
                not_full_cv_.notify_one();
                return true;
            }

            if (diff < 0)
            {
                if (closed_.load(std::memory_order_acquire))
                {
                    return false;
                }

                std::unique_lock<yaclib_std::mutex> lock(not_empty_mutex_);
                not_empty_cv_.wait(lock, [this]() {
                    if (closed_.load(std::memory_order_acquire))
                    {
                        return true;
                    }
                    return IsCurrentSlotReadable();
                });
            }
        }
    }

    void Close() override
    {
        const bool was_closed = closed_.exchange(true, std::memory_order_acq_rel);
        if (!was_closed)
        {
            not_empty_cv_.notify_all();
            not_full_cv_.notify_all();
        }
    }

private:
    struct Slot
    {
        yaclib_std::atomic<std::size_t> sequence{0};
        T value{};
    };

    static std::ptrdiff_t SequenceDiff(std::size_t lhs, std::size_t rhs)
    {
        return static_cast<std::ptrdiff_t>(lhs) - static_cast<std::ptrdiff_t>(rhs);
    }

    std::size_t Index(std::size_t pos) const
    {
        return pos % capacity_;
    }

    bool CanProducerReserveSlot() const
    {
        const auto pos = enqueue_pos_.load(std::memory_order_relaxed);
        const auto seq = ring_[Index(pos)].sequence.load(std::memory_order_acquire);
        return SequenceDiff(seq, pos) >= 0;
    }

    bool IsCurrentSlotReadable() const
    {
        const auto pos = dequeue_pos_;
        const auto seq = ring_[Index(pos)].sequence.load(std::memory_order_acquire);
        return SequenceDiff(seq, pos + 1) == 0;
    }

    std::size_t capacity_;
    std::vector<Slot> ring_;

    yaclib_std::atomic<std::size_t> enqueue_pos_{0};
    std::size_t dequeue_pos_ = 0;
    yaclib_std::atomic<bool> closed_{false};

    mutable yaclib_std::mutex not_empty_mutex_;
    mutable yaclib_std::mutex not_full_mutex_;
    yaclib_std::condition_variable not_empty_cv_;
    yaclib_std::condition_variable not_full_cv_;
};
} // namespace detail

int GeneratePartitionToOwnFile(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    std::int32_t part,
    std::int32_t part_count)
{
    std::unique_ptr<ITableWriter> writer;
    bool partition_open = false;

    try
    {
        auto generator = ctx.registry->CreateGenerator(table.name, ctx.pool);
        if (!generator)
        {
            std::cerr << "Failed to create generator for table: " << table.name << "\n";
            return 2;
        }

        writer = format_driver.CreateWriter();
        if (!writer)
        {
            std::cerr << "Writer is not available for format: " << format_driver.Name() << "\n";
            return 2;
        }

        const PartitionSpec partition{.part_num = part, .part_count = part_count};
        const auto open_status = writer->OpenPartition(table, output, writer_options, partition, ctx.pool);
        if (!open_status.ok())
        {
            std::cerr << "Failed to open partition " << part << "/" << part_count << " for table " << table.name << ": "
                      << open_status.ToString() << "\n";
            return 2;
        }
        partition_open = true;

        GeneratorContext gen_ctx;
        gen_ctx.scale = *ctx.scale;
        gen_ctx.partition = MakePartitionPlan(table, *ctx.scale, part, part_count);
        gen_ctx.text_pool = ctx.text_pool;
        gen_ctx.pool = ctx.pool;
        generator->Reset(gen_ctx);

        TableBatch batch;
        while (generator->NextBatch(ctx.batch_rows, &batch))
        {
            const auto write_status = writer->WriteBatch(batch);
            if (!write_status.ok())
            {
                std::cerr << "Failed to write partition " << part << "/" << part_count << " for table " << table.name << ": "
                          << write_status.ToString() << "\n";
                const auto cleanup_status = writer->ClosePartition();
                partition_open = false;
                if (!cleanup_status.ok())
                {
                    std::cerr << "Partition cleanup failed for table " << table.name << ": " << cleanup_status.ToString() << "\n";
                }
                return 2;
            }
        }

        const auto close_status = writer->ClosePartition();
        partition_open = false;
        if (!close_status.ok())
        {
            std::cerr << "Failed to close partition " << part << "/" << part_count << " for table " << table.name << ": "
                      << close_status.ToString() << "\n";
            return 2;
        }
        return 0;
    }
    catch (const std::exception & ex)
    {
        std::cerr << "Partition " << part << "/" << part_count << " failed for table " << table.name << ": " << ex.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Partition " << part << "/" << part_count << " failed for table " << table.name
                  << ": unknown error during generation\n";
    }

    if (partition_open && writer)
    {
        const auto cleanup_status = writer->ClosePartition();
        if (!cleanup_status.ok())
        {
            std::cerr << "Partition cleanup failed for table " << table.name << ": " << cleanup_status.ToString() << "\n";
        }
    }
    return 2;
}

int RunParallelPartitionFiles(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    std::int32_t part_count,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    (void)scheduler_options.queue_capacity;
    detail::PartitionRangeQueue partition_queue(part_count);
    detail::IQueue<std::int32_t> & partitions_queue = partition_queue;
    yaclib_std::atomic<bool> stop_requested{false};
    yaclib_std::atomic<int> first_error{0};

    const auto workers = ResolveWorkerCount(scheduler_options.worker_count, part_count);
    std::vector<yaclib::FutureOn<void>> worker_futures;
    worker_futures.reserve(workers);

    for (std::uint32_t worker = 0; worker < workers; ++worker)
    {
        worker_futures.emplace_back(yaclib::Run(part_executor, [&]() {
            std::int32_t part = 0;
            while (partitions_queue.Pop(&part))
            {
                if (stop_requested.load(std::memory_order_acquire))
                {
                    continue;
                }

                const auto rc = GeneratePartitionToOwnFile(ctx, table, output, writer_options, format_driver, part, part_count);
                if (rc != 0)
                {
                    first_error.store(rc, std::memory_order_release);
                    stop_requested.store(true, std::memory_order_release);
                    partitions_queue.Close();
                    break;
                }
            }
        }));
    }

    for (auto & worker_future : worker_futures)
    {
        auto worker_result = std::move(worker_future).Get();
        try
        {
            (void)std::move(worker_result).Ok();
        }
        catch (...)
        {
            first_error.store(2, std::memory_order_release);
        }
    }

    return first_error.load(std::memory_order_acquire);
}

std::string JoinPath(const std::string & base, const std::string & leaf)
{
    if (base.empty())
    {
        return leaf;
    }
    if (leaf.empty())
    {
        return base;
    }
    if (base.back() == '/')
    {
        return base + leaf;
    }
    return base + "/" + leaf;
}

class ISingleFileSink
{
public:
    virtual ~ISingleFileSink() = default;
    virtual arrow::Status BeginPartition() = 0;
    virtual arrow::Status WriteBatch(const TableBatch & batch) = 0;
    virtual arrow::Status EndPartition() = 0;
    virtual arrow::Status Close() = 0;
};

#if defined(ARROW_PARQUET)
class ParquetSingleFileSink final : public ISingleFileSink
{
public:
    arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        const ParquetWriterOptions & options,
        arrow::MemoryPool * pool)
    {
        table_ = &table;
        pool_ = pool != nullptr ? pool : arrow::default_memory_pool();
        options_ = options;

        ARROW_ASSIGN_OR_RAISE(fs_, ParquetTableWriter::GetFilesystem(output.uri));
        ARROW_ASSIGN_OR_RAISE(const std::string base_dir, ParquetTableWriter::GetPath(output.uri, *fs_));

        const auto table_dir = JoinPath(base_dir, table.name);
        RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

        final_path_ = JoinPath(table_dir, table.name + "-1.parquet");
        temp_path_ = final_path_ + ".tmp";

        ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(final_path_));
        if (info.type() != arrow::fs::FileType::NotFound)
        {
            return arrow::Status::AlreadyExists(final_path_);
        }

        ARROW_ASSIGN_OR_RAISE(sink_, fs_->OpenOutputStream(temp_path_));
        if (options_.output_buffer_bytes > 0)
        {
            ARROW_ASSIGN_OR_RAISE(
                sink_,
                arrow::io::BufferedOutputStream::Create(options_.output_buffer_bytes, pool_, std::move(sink_)));
        }

        auto props_builder = ::parquet::WriterProperties::Builder();
        props_builder.compression(options_.compression);
        const auto row_group_rows = ParquetTableWriter::ResolveRowGroupRows(*table_, options_);
        if (row_group_rows > 0)
        {
            props_builder.max_row_group_length(row_group_rows);
        }
        auto props = props_builder.build();

        auto arrow_props = ::parquet::ArrowWriterProperties::Builder().set_use_threads(options_.use_threads)->build();

        ARROW_ASSIGN_OR_RAISE(
            writer_, ::parquet::arrow::FileWriter::Open(*table.schema, pool_, sink_, std::move(props), std::move(arrow_props)));

        is_open_ = true;
        return arrow::Status::OK();
    }

    arrow::Status BeginPartition() override
    {
        if (!is_open_ || !writer_)
        {
            return arrow::Status::Invalid("ParquetSingleFileSink::BeginPartition() called before Open()");
        }
        if (partition_open_)
        {
            return arrow::Status::OK();
        }
        RETURN_NOT_OK(writer_->NewBufferedRowGroup());
        partition_open_ = true;
        return arrow::Status::OK();
    }

    arrow::Status WriteBatch(const TableBatch & batch) override
    {
        if (!is_open_ || !writer_ || table_ == nullptr)
        {
            return arrow::Status::Invalid("ParquetSingleFileSink::WriteBatch() called before Open()");
        }
        if (batch.metadata == nullptr || batch.metadata != table_)
        {
            return arrow::Status::Invalid("TableBatch metadata mismatch");
        }
        if (batch.row_count == 0)
        {
            return arrow::Status::OK();
        }
        if (!partition_open_)
        {
            RETURN_NOT_OK(BeginPartition());
        }

        const auto row_count = static_cast<std::int64_t>(batch.row_count);
        auto rb = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
        return writer_->WriteRecordBatch(*rb);
    }

    arrow::Status EndPartition() override
    {
        partition_open_ = false;
        return arrow::Status::OK();
    }

    arrow::Status Close() override
    {
        if (!is_open_)
        {
            return arrow::Status::OK();
        }

        RETURN_NOT_OK(writer_->Close());
        writer_.reset();

        if (sink_)
        {
            RETURN_NOT_OK(sink_->Close());
            sink_.reset();
        }

        if (!temp_path_.empty() && !final_path_.empty())
        {
            RETURN_NOT_OK(fs_->Move(temp_path_, final_path_));
        }

        fs_.reset();
        table_ = nullptr;
        pool_ = nullptr;
        temp_path_.clear();
        final_path_.clear();
        is_open_ = false;
        partition_open_ = false;
        return arrow::Status::OK();
    }

private:
    const TableMetadata * table_ = nullptr;
    arrow::MemoryPool * pool_ = nullptr;
    ParquetWriterOptions options_{};
    FileSystemPtr fs_;
    std::string temp_path_;
    std::string final_path_;
    std::shared_ptr<arrow::io::OutputStream> sink_;
    std::unique_ptr<::parquet::arrow::FileWriter> writer_;
    bool is_open_ = false;
    bool partition_open_ = false;
};
#endif

#if defined(ARROW_ORC)
class OrcSingleFileSink final : public ISingleFileSink
{
public:
    arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        const OrcWriterOptions & options,
        arrow::MemoryPool * pool)
    {
        table_ = &table;
        pool_ = pool != nullptr ? pool : arrow::default_memory_pool();
        options_ = options;

        ARROW_ASSIGN_OR_RAISE(fs_, OrcTableWriter::GetFilesystem(output.uri));
        ARROW_ASSIGN_OR_RAISE(const std::string base_dir, OrcTableWriter::GetPath(output.uri, *fs_));

        const auto table_dir = JoinPath(base_dir, table.name);
        RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

        final_path_ = JoinPath(table_dir, table.name + "-1.orc");
        temp_path_ = final_path_ + ".tmp";

        ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(final_path_));
        if (info.type() != arrow::fs::FileType::NotFound)
        {
            return arrow::Status::AlreadyExists(final_path_);
        }

        ARROW_ASSIGN_OR_RAISE(sink_, fs_->OpenOutputStream(temp_path_));
        if (options_.output_buffer_bytes > 0)
        {
            ARROW_ASSIGN_OR_RAISE(
                sink_,
                arrow::io::BufferedOutputStream::Create(options_.output_buffer_bytes, pool_, std::move(sink_)));
        }

        arrow::adapters::orc::WriteOptions write_options;
        const auto stripe_bytes = OrcTableWriter::ResolveStripeBytes(*table_, options_);
        if (stripe_bytes > 0)
        {
            write_options.stripe_size = stripe_bytes;
        }
        write_options.compression = options_.compression;

        ARROW_ASSIGN_OR_RAISE(writer_, arrow::adapters::orc::ORCFileWriter::Open(sink_.get(), write_options));

        is_open_ = true;
        return arrow::Status::OK();
    }

    arrow::Status BeginPartition() override
    {
        if (!is_open_ || !writer_)
        {
            return arrow::Status::Invalid("OrcSingleFileSink::BeginPartition() called before Open()");
        }
        /* 
         * Note: Arrow adapter has no explicit "start stripe" API
         * keep partition state for scheduler ordering 
         */
        partition_open_ = true;
        return arrow::Status::OK();
    }

    arrow::Status WriteBatch(const TableBatch & batch) override
    {
        if (!is_open_ || !writer_ || table_ == nullptr)
        {
            return arrow::Status::Invalid("OrcSingleFileSink::WriteBatch() called before Open()");
        }
        if (batch.metadata == nullptr || batch.metadata != table_)
        {
            return arrow::Status::Invalid("TableBatch metadata mismatch");
        }
        if (batch.row_count == 0)
        {
            return arrow::Status::OK();
        }
        if (!partition_open_)
        {
            RETURN_NOT_OK(BeginPartition());
        }

        const auto row_count = static_cast<std::int64_t>(batch.row_count);
        auto rb = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
        return writer_->Write(*rb);
    }

    arrow::Status EndPartition() override
    {
        partition_open_ = false;
        return arrow::Status::OK();
    }

    arrow::Status Close() override
    {
        if (!is_open_)
        {
            return arrow::Status::OK();
        }

        RETURN_NOT_OK(writer_->Close());
        writer_.reset();

        if (sink_)
        {
            RETURN_NOT_OK(sink_->Close());
            sink_.reset();
        }

        if (!temp_path_.empty() && !final_path_.empty())
        {
            RETURN_NOT_OK(fs_->Move(temp_path_, final_path_));
        }

        fs_.reset();
        table_ = nullptr;
        pool_ = nullptr;
        temp_path_.clear();
        final_path_.clear();
        is_open_ = false;
        partition_open_ = false;
        return arrow::Status::OK();
    }

private:
    const TableMetadata * table_ = nullptr;
    arrow::MemoryPool * pool_ = nullptr;
    OrcWriterOptions options_{};
    FileSystemPtr fs_;
    std::string temp_path_;
    std::string final_path_;
    std::shared_ptr<arrow::io::OutputStream> sink_;
    std::unique_ptr<arrow::adapters::orc::ORCFileWriter> writer_;
    bool is_open_ = false;
    bool partition_open_ = false;
};
#endif

arrow::Result<std::unique_ptr<ISingleFileSink>> OpenSingleFileSink(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    arrow::MemoryPool * pool)
{
    switch (writer_options.format)
    {
        case OutputFormat::Parquet:
#if defined(ARROW_PARQUET)
        {
            const auto & parquet_options = ParquetTableWriter::ResolveOptions(writer_options);
            auto sink = std::make_unique<ParquetSingleFileSink>();
            RETURN_NOT_OK(sink->Open(table, output, parquet_options, pool));
            return sink;
        }
#else
            return arrow::Status::NotImplemented("single-file-ordered requires ARROW_PARQUET for parquet format");
#endif
        case OutputFormat::Orc:
#if defined(ARROW_ORC)
        {
            const auto & orc_options = OrcTableWriter::ResolveOptions(writer_options);
            auto sink = std::make_unique<OrcSingleFileSink>();
            RETURN_NOT_OK(sink->Open(table, output, orc_options, pool));
            return sink;
        }
#else
            return arrow::Status::NotImplemented("single-file-ordered requires ARROW_ORC for orc format");
#endif
        default:
            return arrow::Status::NotImplemented(
                "single-file-ordered strategy is currently supported for parquet and orc formats");
    }
}
enum class BatchMessageKind : std::uint8_t
{
    Batch,
    EndPartition,
    Error,
};

struct BatchMessage
{
    BatchMessageKind kind = BatchMessageKind::Batch;
    std::int32_t part = 1;
    TableBatch batch;
    std::string error;
};

int RunSingleFileOrdered(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    std::int32_t part_count,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    auto writer = format_driver.CreateOrderedWriter();
    std::unique_ptr<ISingleFileSink> sink;
    const bool use_sink_fallback = writer == nullptr;

    if (use_sink_fallback)
    {
        auto sink_result = OpenSingleFileSink(table, output, writer_options, ctx.pool);
        if (!sink_result.ok())
        {
            std::cerr << "Failed to open single output file for table " << table.name << " (format " << format_driver.Name()
                      << "): " << sink_result.status().ToString() << "\n";
            return 2;
        }
        sink = std::move(sink_result).ValueOrDie();
    }
    else
    {
        const auto open_status = writer->OpenTable(table, output, writer_options, ctx.pool);
        if (!open_status.ok())
        {
            std::cerr << "Failed to open ordered output for table " << table.name << ": " << open_status.ToString() << "\n";
            return 2;
        }
    }

    detail::BoundedMpscQueue<BatchMessage> queue(scheduler_options.queue_capacity);
    yaclib_std::atomic<bool> stop_requested{false};
    yaclib_std::atomic<std::int32_t> remaining_producers{part_count};

    std::vector<yaclib::FutureOn<int>> producer_futures;
    producer_futures.reserve(static_cast<std::size_t>(part_count));

    for (std::int32_t part = 1; part <= part_count; ++part)
    {
        producer_futures.emplace_back(yaclib::Run(part_executor, [&, part]() -> int {
            try
            {
                auto generator = ctx.registry->CreateGenerator(table.name, ctx.pool);
                if (!generator)
                {
                    BatchMessage error;
                    error.kind = BatchMessageKind::Error;
                    error.part = part;
                    error.error = "Failed to create generator for table: " + table.name;
                    (void)queue.Push(std::move(error));
                    stop_requested.store(true, std::memory_order_release);
                    queue.Close();
                    return 2;
                }

                GeneratorContext gen_ctx;
                gen_ctx.scale = *ctx.scale;
                gen_ctx.partition = MakePartitionPlan(table, *ctx.scale, part, part_count);
                gen_ctx.text_pool = ctx.text_pool;
                gen_ctx.pool = ctx.pool;
                generator->Reset(gen_ctx);

                while (!stop_requested.load(std::memory_order_acquire))
                {
                    TableBatch batch;
                    if (!generator->NextBatch(ctx.batch_rows, &batch))
                    {
                        break;
                    }
                    BatchMessage message;
                    message.kind = BatchMessageKind::Batch;
                    message.part = part;
                    message.batch = std::move(batch);
                    if (!queue.Push(std::move(message)))
                    {
                        return 2;
                    }
                }

                if (!stop_requested.load(std::memory_order_acquire))
                {
                    BatchMessage end;
                    end.kind = BatchMessageKind::EndPartition;
                    end.part = part;
                    (void)queue.Push(std::move(end));
                }
            }
            catch (const std::exception & ex)
            {
                BatchMessage error;
                error.kind = BatchMessageKind::Error;
                error.part = part;
                error.error = ex.what();
                (void)queue.Push(std::move(error));
                stop_requested.store(true, std::memory_order_release);
                queue.Close();
                return 2;
            }
            catch (...)
            {
                BatchMessage error;
                error.kind = BatchMessageKind::Error;
                error.part = part;
                error.error = "Unknown partition generation error";
                (void)queue.Push(std::move(error));
                stop_requested.store(true, std::memory_order_release);
                queue.Close();
                return 2;
            }

            if (remaining_producers.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                queue.Close();
            }
            return 0;
        }));
    }

    struct PartitionState
    {
        std::deque<TableBatch> batches;
        bool completed = false;
        bool opened = false;
    };

    std::unordered_map<std::int32_t, PartitionState> states;
    states.reserve(static_cast<std::size_t>(part_count));

    std::int32_t expected_part = 1;
    std::string pipeline_error;

    auto drain_ready_partitions = [&]() -> arrow::Status {
        while (expected_part <= part_count)
        {
            auto state_it = states.find(expected_part);
            if (state_it == states.end())
            {
                break;
            }

            auto & state = state_it->second;
            if (!state.batches.empty())
            {
                if (!state.opened)
                {
                    if (use_sink_fallback)
                    {
                        RETURN_NOT_OK(sink->BeginPartition());
                    }
                    else
                    {
                        const PartitionSpec partition{.part_num = expected_part, .part_count = part_count};
                        RETURN_NOT_OK(writer->BeginInputPartition(partition));
                    }
                    state.opened = true;
                }

                while (!state.batches.empty())
                {
                    if (use_sink_fallback)
                    {
                        RETURN_NOT_OK(sink->WriteBatch(state.batches.front()));
                    }
                    else
                    {
                        RETURN_NOT_OK(writer->WriteBatch(state.batches.front()));
                    }
                    state.batches.pop_front();
                }
            }

            if (!state.completed)
            {
                break;
            }

            if (state.opened)
            {
                if (use_sink_fallback)
                {
                    RETURN_NOT_OK(sink->EndPartition());
                }
                else
                {
                    RETURN_NOT_OK(writer->EndInputPartition());
                }
            }

            states.erase(state_it);
            ++expected_part;
        }
        return arrow::Status::OK();
    };

    BatchMessage message;
    while (queue.Pop(&message))
    {
        if (message.kind == BatchMessageKind::Error)
        {
            if (pipeline_error.empty())
            {
                pipeline_error = "Partition " + std::to_string(message.part) + " failed: " + message.error;
            }
            stop_requested.store(true, std::memory_order_release);
            queue.Close();
            continue;
        }

        auto & state = states[message.part];
        if (message.kind == BatchMessageKind::Batch)
        {
            state.batches.push_back(std::move(message.batch));
        }
        else
        {
            state.completed = true;
        }

        const auto drain_status = drain_ready_partitions();
        if (!drain_status.ok())
        {
            pipeline_error = drain_status.ToString();
            stop_requested.store(true, std::memory_order_release);
            queue.Close();
        }
    }

    for (auto & future : producer_futures)
    {
        auto result = std::move(future).Get();
        try
        {
            const int rc = std::move(result).Ok();
            if (rc != 0 && pipeline_error.empty())
            {
                pipeline_error = "Partition producer returned error";
            }
        }
        catch (const std::exception & ex)
        {
            if (pipeline_error.empty())
            {
                pipeline_error = ex.what();
            }
        }
    }

    const auto final_drain_status = drain_ready_partitions();
    if (!final_drain_status.ok() && pipeline_error.empty())
    {
        pipeline_error = final_drain_status.ToString();
    }

    if (expected_part != part_count + 1 && pipeline_error.empty())
    {
        pipeline_error = "Not all partitions were written to the output file";
    }

    const auto close_status = use_sink_fallback ? sink->Close() : writer->CloseTable();
    if (!close_status.ok() && pipeline_error.empty())
    {
        pipeline_error = close_status.ToString();
    }

    if (!pipeline_error.empty())
    {
        std::cerr << "single-file-ordered write failed for table " << table.name << ": " << pipeline_error << "\n";
        return 2;
    }
    return 0;
}
} // namespace

void RegisterWriteSchedulerCliOptions(po::options_description & desc)
{
    desc.add_options()(
        "write-strategy",
        po::value<std::string>()->default_value("auto"),
        "Write strategy: auto | parallel-files | single-file-ordered")(
        "write-workers",
        po::value<std::uint32_t>()->default_value(0),
        "Worker count for partition processing (0 = auto)")(
        "write-queue-capacity",
        po::value<std::size_t>()->default_value(8),
        "Bounded queue capacity for scheduler data flow");
}

arrow::Result<WriteSchedulerOptions> BuildWriteSchedulerOptions(const po::variables_map & vm)
{
    WriteSchedulerOptions options;
    const auto strategy = ToLower(vm["write-strategy"].as<std::string>());
    if (strategy == "auto")
    {
        options.strategy.reset();
    }
    else if (strategy == "parallel-files")
    {
        options.strategy = WriteStrategy::ParallelPartitionFiles;
    }
    else if (strategy == "single-file-ordered")
    {
        options.strategy = WriteStrategy::SingleFileOrdered;
    }
    else
    {
        return arrow::Status::Invalid("Unsupported write-strategy: ", strategy);
    }

    options.worker_count = vm["write-workers"].as<std::uint32_t>();
    options.queue_capacity = vm["write-queue-capacity"].as<std::size_t>();
    if (options.queue_capacity == 0)
    {
        return arrow::Status::Invalid("write-queue-capacity must be > 0");
    }
    return options;
}

arrow::Result<WriteStrategy> ResolveSelectedWriteStrategy(
    const WriteSchedulerOptions & options,
    const IFormatDriver & format_driver)
{
    const auto strategy = options.strategy.has_value() ? *options.strategy : format_driver.PreferredStrategy();
    if (!format_driver.SupportsStrategy(strategy))
    {
        return arrow::Status::Invalid(
            "Write strategy is not supported for format ",
            std::string(format_driver.Name()),
            ": ",
            strategy == WriteStrategy::ParallelPartitionFiles ? "parallel-files" : "single-file-ordered");
    }
    return strategy;
}

int GenerateTableWithStrategy(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    const auto part_count = format_driver.ResolvePartCount(table, *ctx.scale, writer_options);
    if (part_count < 1)
    {
        std::cerr << "Invalid part_count for table " << table.name << "\n";
        return 2;
    }

    auto selected_strategy_result = ResolveSelectedWriteStrategy(scheduler_options, format_driver);
    if (!selected_strategy_result.ok())
    {
        std::cerr << selected_strategy_result.status().ToString() << "\n";
        return 2;
    }
    const auto strategy = std::move(selected_strategy_result).ValueOrDie();

    switch (strategy)
    {
        case WriteStrategy::ParallelPartitionFiles:
            return RunParallelPartitionFiles(
                ctx, table, output, writer_options, format_driver, part_count, scheduler_options, part_executor);
        case WriteStrategy::SingleFileOrdered:
            return RunSingleFileOrdered(
                ctx, table, output, writer_options, format_driver, part_count, scheduler_options, part_executor);
        default:
            return 2;
    }
}
