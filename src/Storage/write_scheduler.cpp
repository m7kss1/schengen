#include "Storage/write_scheduler.h"

#include "Storage/progress.h"
#include "Storage/orc_writer.h"
#include "Storage/parquet_writer.h"
#if defined(ARROW_PARQUET)
#  include "arrow/io/memory.h"
#  include "generated/parquet_types.h"
#  include "parquet/thrift_internal.h"
#endif
#include <boost/program_options.hpp>
#include <arrow/io/buffered.h>

#include "yaclib/async/future.hpp"
#include "yaclib/async/run.hpp"
#include "yaclib_std/atomic"
#include "yaclib_std/condition_variable"
#include "yaclib_std/mutex"
#include "yaclib_std/thread"

#include <algorithm>
#include <cctype>
#include <deque>
#include <iostream>
#include <limits>
#include <sstream>
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

bool HasProgress(const GenerationContext & ctx)
{
    return ctx.progress != nullptr && ctx.progress_table_index != std::numeric_limits<std::size_t>::max();
}

void MarkTableStarted(const GenerationContext & ctx, std::int32_t part_count)
{
    if (HasProgress(ctx))
    {
        ctx.progress->MarkTableStarted(ctx.progress_table_index, part_count);
    }
}

void AddCommittedRows(const GenerationContext & ctx, std::uint64_t rows)
{
    if (HasProgress(ctx))
    {
        ctx.progress->AddCommittedRows(ctx.progress_table_index, rows);
    }
}

void MarkPartCompleted(const GenerationContext & ctx)
{
    if (HasProgress(ctx))
    {
        ctx.progress->MarkPartCompleted(ctx.progress_table_index);
    }
}

void MarkTableFinished(const GenerationContext & ctx)
{
    if (HasProgress(ctx))
    {
        ctx.progress->MarkTableFinished(ctx.progress_table_index);
    }
}

void ReportError(const GenerationContext & ctx, std::string message)
{
    if (HasProgress(ctx))
    {
        ctx.progress->ReportFailure(ctx.progress_table_index, std::move(message));
        return;
    }
    std::cerr << message << "\n";
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

#if defined(ARROW_PARQUET)
struct InMemoryPartitionData
{
    std::int32_t part = 0;
    std::shared_ptr<arrow::Buffer> buffer;
    std::string error;
};
#endif

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
            ReportError(ctx, "Failed to create generator for table: " + table.name);
            return 2;
        }

        writer = format_driver.CreateWriter();
        if (!writer)
        {
            ReportError(ctx, "Writer is not available for format: " + std::string(format_driver.Name()));
            return 2;
        }

        const PartitionSpec partition{.part_num = part, .part_count = part_count};
        const auto open_status = writer->OpenPartition(table, output, writer_options, partition, ctx.pool);
        if (!open_status.ok())
        {
            std::ostringstream message;
            message << "Failed to open partition " << part << "/" << part_count << " for table " << table.name << ": "
                    << open_status.ToString();
            ReportError(ctx, message.str());
            return 2;
        }
        partition_open = true;

        GeneratorContext gen_ctx;
        gen_ctx.scale = *ctx.scale;
        gen_ctx.partition = MakePartitionPlan(table, *ctx.scale, part, part_count);
        gen_ctx.text_pool = ctx.text_pool;
        gen_ctx.pool = ctx.pool;
        generator->Reset(gen_ctx);

        /*
         * SPSC pipeline: generator runs on a dedicated yaclib_std::thread,
         * writer runs in the current thread. Capacity = 2 lets the
         * writer encode/compress one batch while the next is being
         * generated, hiding either stage when both are CPU-bound.
         *
         * A dedicated thread is used (not yaclib::Run on part_executor)
         * to avoid a deadlock risk: callers of GeneratePartitionToOwnFile
         * are typically RunParallelPartitionFiles workers already occupying
         * the part_executor pool. Spawning the producer on the same pool
         * could starve when all pool threads block on Pop awaiting a
         * queued producer that never gets a runner. yaclib_std::thread
         * is the same idiom used by CliProgressController::render_thread_.
         */
        constexpr std::size_t kPipelineCapacity = 2;
        detail::BoundedMpscQueue<BatchMessage> pipeline_queue(kPipelineCapacity);
        yaclib_std::atomic<bool> stop_requested{false};

        yaclib_std::thread producer_thread([&]() {
            try
            {
                while (!stop_requested.load(std::memory_order_acquire))
                {
                    TableBatch batch;
                    if (!generator->NextBatch(ctx.batch_rows, &batch))
                    {
                        break;
                    }
                    BatchMessage msg;
                    msg.kind = BatchMessageKind::Batch;
                    msg.part = part;
                    msg.batch = std::move(batch);
                    if (!pipeline_queue.Push(std::move(msg)))
                    {
                        break;
                    }
                }
            }
            catch (const std::exception & ex)
            {
                BatchMessage err;
                err.kind = BatchMessageKind::Error;
                err.part = part;
                err.error = ex.what();
                (void)pipeline_queue.Push(std::move(err));
            }
            catch (...)
            {
                BatchMessage err;
                err.kind = BatchMessageKind::Error;
                err.part = part;
                err.error = "Unknown generation error";
                (void)pipeline_queue.Push(std::move(err));
            }
            pipeline_queue.Close();
        });

        std::string pipeline_error;
        BatchMessage msg;
        while (pipeline_queue.Pop(&msg))
        {
            if (msg.kind == BatchMessageKind::Error)
            {
                if (pipeline_error.empty())
                {
                    pipeline_error = msg.error;
                }
                stop_requested.store(true, std::memory_order_release);
                pipeline_queue.Close();
                continue;
            }
            if (stop_requested.load(std::memory_order_acquire))
            {
                continue;
            }
            const auto write_status = writer->WriteBatch(msg.batch);
            if (!write_status.ok())
            {
                if (pipeline_error.empty())
                {
                    pipeline_error = write_status.ToString();
                }
                stop_requested.store(true, std::memory_order_release);
                pipeline_queue.Close();
                continue;
            }
            AddCommittedRows(ctx, msg.batch.row_count);
        }

        producer_thread.join();

        if (!pipeline_error.empty())
        {
            std::ostringstream message;
            message << "Failed to write partition " << part << "/" << part_count << " for table " << table.name << ": "
                    << pipeline_error;
            ReportError(ctx, message.str());
            const auto cleanup_status = writer->ClosePartition();
            partition_open = false;
            if (!cleanup_status.ok())
            {
                ReportError(ctx, "Partition cleanup failed for table " + table.name + ": " + cleanup_status.ToString());
            }
            return 2;
        }

        const auto close_status = writer->ClosePartition();
        partition_open = false;
        if (!close_status.ok())
        {
            std::ostringstream message;
            message << "Failed to close partition " << part << "/" << part_count << " for table " << table.name << ": "
                    << close_status.ToString();
            ReportError(ctx, message.str());
            return 2;
        }
        MarkPartCompleted(ctx);
        return 0;
    }
    catch (const std::exception & ex)
    {
        std::ostringstream message;
        message << "Partition " << part << "/" << part_count << " failed for table " << table.name << ": " << ex.what();
        ReportError(ctx, message.str());
    }
    catch (...)
    {
        std::ostringstream message;
        message << "Partition " << part << "/" << part_count << " failed for table " << table.name
                << ": unknown error during generation";
        ReportError(ctx, message.str());
    }

    if (partition_open && writer)
    {
        const auto cleanup_status = writer->ClosePartition();
        if (!cleanup_status.ok())
        {
            ReportError(ctx, "Partition cleanup failed for table " + table.name + ": " + cleanup_status.ToString());
        }
    }
    return 2;
}

#if defined(ARROW_PARQUET)
arrow::Result<std::shared_ptr<arrow::Buffer>> GeneratePartitionToInMemory(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const ParquetWriterOptions & options,
    std::int32_t part,
    std::int32_t part_count)
{
    ARROW_ASSIGN_OR_RAISE(auto buf_stream, arrow::io::BufferOutputStream::Create(0, ctx.pool));

    auto props_builder = ::parquet::WriterProperties::Builder();
    props_builder.compression(options.compression);
    const auto row_group_rows = ParquetTableWriter::ResolveRowGroupRows(table, options);
    if (row_group_rows > 0)
        props_builder.max_row_group_length(row_group_rows);
    auto arrow_props = ::parquet::ArrowWriterProperties::Builder().set_use_threads(false)->build();

    ARROW_ASSIGN_OR_RAISE(
        auto writer,
        ::parquet::arrow::FileWriter::Open(
            *table.schema, ctx.pool, buf_stream, props_builder.build(), std::move(arrow_props)));

    auto generator = ctx.registry->CreateGenerator(table.name, ctx.pool);
    if (!generator)
        return arrow::Status::ExecutionError("Failed to create generator for table: " + table.name);

    GeneratorContext gen_ctx;
    gen_ctx.scale = *ctx.scale;
    gen_ctx.partition = MakePartitionPlan(table, *ctx.scale, part, part_count);
    gen_ctx.text_pool = ctx.text_pool;
    gen_ctx.pool = ctx.pool;
    generator->Reset(gen_ctx);

    constexpr std::size_t kPipelineCapacity = 2;
    detail::BoundedMpscQueue<BatchMessage> pipeline_queue(kPipelineCapacity);
    yaclib_std::atomic<bool> stop_requested{false};

    yaclib_std::thread producer_thread([&]() {
        try
        {
            while (!stop_requested.load(std::memory_order_acquire))
            {
                TableBatch batch;
                if (!generator->NextBatch(ctx.batch_rows, &batch))
                    break;
                BatchMessage msg;
                msg.kind = BatchMessageKind::Batch;
                msg.part = part;
                msg.batch = std::move(batch);
                if (!pipeline_queue.Push(std::move(msg)))
                    break;
            }
        }
        catch (const std::exception & ex)
        {
            BatchMessage err;
            err.kind = BatchMessageKind::Error;
            err.part = part;
            err.error = ex.what();
            (void)pipeline_queue.Push(std::move(err));
        }
        catch (...)
        {
            BatchMessage err;
            err.kind = BatchMessageKind::Error;
            err.part = part;
            err.error = "Unknown generation error";
            (void)pipeline_queue.Push(std::move(err));
        }
        pipeline_queue.Close();
    });

    std::string pipeline_error;
    bool row_group_open = false;
    BatchMessage msg;
    while (pipeline_queue.Pop(&msg))
    {
        if (msg.kind == BatchMessageKind::Error)
        {
            if (pipeline_error.empty())
                pipeline_error = msg.error;
            stop_requested.store(true, std::memory_order_release);
            pipeline_queue.Close();
            continue;
        }
        if (stop_requested.load(std::memory_order_acquire))
            continue;

        if (!row_group_open)
        {
            const auto rg_status = writer->NewBufferedRowGroup();
            if (!rg_status.ok())
            {
                if (pipeline_error.empty())
                    pipeline_error = rg_status.ToString();
                stop_requested.store(true, std::memory_order_release);
                pipeline_queue.Close();
                continue;
            }
            row_group_open = true;
        }

        const auto row_count = static_cast<std::int64_t>(msg.batch.row_count);
        auto rb = arrow::RecordBatch::Make(table.schema, row_count, msg.batch.columns);
        const auto write_status = writer->WriteRecordBatch(*rb);
        if (!write_status.ok())
        {
            if (pipeline_error.empty())
                pipeline_error = write_status.ToString();
            stop_requested.store(true, std::memory_order_release);
            pipeline_queue.Close();
        }
    }

    producer_thread.join();

    if (!pipeline_error.empty())
        return arrow::Status::ExecutionError(pipeline_error);

    RETURN_NOT_OK(writer->Close());
    return buf_stream->Finish();
}
#endif

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

#if defined(ARROW_PARQUET)
int RunParallelEncodeParquetOrdered(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const ParquetWriterOptions & options,
    std::int32_t part_count,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    auto fail = [&](const std::string & message) -> int {
        ReportError(ctx, "parallel-encode-parquet write failed for table " + table.name + ": " + message);
        return 2;
    };

    auto fs_result = ParquetTableWriter::GetFilesystem(output.uri);
    if (!fs_result.ok())
        return fail(fs_result.status().ToString());
    auto fs = std::move(fs_result).ValueOrDie();

    auto base_dir_result = ParquetTableWriter::GetPath(output.uri, *fs);
    if (!base_dir_result.ok())
        return fail(base_dir_result.status().ToString());
    const auto base_dir = std::move(base_dir_result).ValueOrDie();

    const auto table_dir = JoinPath(base_dir, table.name);
    const auto create_status = fs->CreateDir(table_dir, /*recursive=*/true);
    if (!create_status.ok())
        return fail(create_status.ToString());

    const auto final_path = JoinPath(table_dir, table.name + "-1.parquet");
    const auto temp_path = final_path + ".tmp";

    const auto info_result = fs->GetFileInfo(final_path);
    if (info_result.ok() && info_result.ValueOrDie().type() != arrow::fs::FileType::NotFound)
    {
        ReportError(ctx, final_path + " already exists, skipping generation");
        return 0;
    }

    auto open_result = fs->OpenOutputStream(temp_path);
    if (!open_result.ok())
        return fail(open_result.status().ToString());
    std::shared_ptr<arrow::io::OutputStream> sink = std::move(open_result).ValueOrDie();

    if (options.output_buffer_bytes > 0)
    {
        auto buf_result = arrow::io::BufferedOutputStream::Create(options.output_buffer_bytes, ctx.pool, sink);
        if (!buf_result.ok())
            return fail(buf_result.status().ToString());
        sink = std::move(buf_result).ValueOrDie();
    }

    // Parallel encode: each partition independently encodes to an in-memory Parquet mini-file.
    // The consumer merges them in order by adjusting column offsets and concatenating the row
    // group data, then writes a single merged Thrift footer.
    detail::PartitionRangeQueue partition_queue(part_count);
    const auto worker_count = ResolveWorkerCount(scheduler_options.worker_count, part_count);
    detail::BoundedMpscQueue<InMemoryPartitionData> result_queue(worker_count);
    yaclib_std::atomic<bool> stop_requested{false};
    yaclib_std::atomic<int> remaining_workers{static_cast<int>(worker_count)};

    std::vector<yaclib::FutureOn<void>> worker_futures;
    worker_futures.reserve(worker_count);
    for (std::uint32_t w = 0; w < worker_count; ++w)
    {
        worker_futures.emplace_back(yaclib::Run(part_executor, [&]() {
            std::int32_t part = 0;
            while (!stop_requested.load(std::memory_order_acquire) && partition_queue.Pop(&part))
            {
                InMemoryPartitionData result;
                result.part = part;
                auto buf_result = GeneratePartitionToInMemory(ctx, table, options, part, part_count);
                if (!buf_result.ok())
                {
                    result.error = buf_result.status().ToString();
                    stop_requested.store(true, std::memory_order_release);
                }
                else
                {
                    result.buffer = std::move(buf_result).ValueOrDie();
                }
                if (!result_queue.Push(std::move(result)))
                    break;
            }
            if (remaining_workers.fetch_sub(1, std::memory_order_acq_rel) == 1)
                result_queue.Close();
        }));
    }

    // Consumer: collect results in arrival order, process them in partition order.
    // Column offsets in each mini-file are relative to that file's beginning (offset 4 after
    // the PAR1 magic). We adjust them by delta = cumulative row-group-data size written so far.
    static constexpr uint8_t kParquetMagic[4] = {'P', 'A', 'R', '1'};
    std::string pipeline_error;

    {
        const auto ws = sink->Write(kParquetMagic, 4);
        if (!ws.ok())
            pipeline_error = ws.ToString();
    }

    parquet::format::FileMetaData merged_metadata;
    merged_metadata.num_rows = 0;
    bool schema_set = false;
    std::int64_t current_output_data_offset = 0;
    ::parquet::ThriftDeserializer deserializer(
        std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max());

    auto process_buffer = [&](const std::shared_ptr<arrow::Buffer> & buffer, std::int32_t part_num) {
        if (!pipeline_error.empty())
            return;
        const std::int64_t file_size = buffer->size();
        const uint8_t * data = buffer->data();

        const uint8_t * footer_suffix = data + file_size - 8;
        const auto footer_len = static_cast<std::uint32_t>(footer_suffix[0])
            | (static_cast<std::uint32_t>(footer_suffix[1]) << 8)
            | (static_cast<std::uint32_t>(footer_suffix[2]) << 16)
            | (static_cast<std::uint32_t>(footer_suffix[3]) << 24);
        const std::int64_t footer_start = file_size - 8 - static_cast<std::int64_t>(footer_len);
        const std::int64_t row_group_data_size = footer_start - 4;

        ::parquet::format::FileMetaData file_meta;
        std::uint32_t meta_len = footer_len;
        try
        {
            deserializer.DeserializeMessage(data + footer_start, &meta_len, &file_meta);
        }
        catch (const std::exception & ex)
        {
            pipeline_error = "footer deserialization failed for partition "
                + std::to_string(part_num) + ": " + ex.what();
            return;
        }

        if (!schema_set)
        {
            merged_metadata.version = file_meta.version;
            merged_metadata.schema = file_meta.schema;
            if (file_meta.__isset.created_by)
                merged_metadata.__set_created_by(file_meta.created_by);
            schema_set = true;
        }

        // Shift all absolute file offsets by delta (= bytes written to output data section so far).
        const std::int64_t delta = current_output_data_offset;
        for (auto & rg : file_meta.row_groups)
        {
            for (auto & col : rg.columns)
            {
                if (col.__isset.meta_data)
                {
                    auto & md = col.meta_data;
                    md.__set_data_page_offset(md.data_page_offset + delta);
                    if (md.__isset.index_page_offset)
                        md.__set_index_page_offset(md.index_page_offset + delta);
                    if (md.__isset.dictionary_page_offset)
                        md.__set_dictionary_page_offset(md.dictionary_page_offset + delta);
                    if (md.__isset.bloom_filter_offset)
                        md.__set_bloom_filter_offset(md.bloom_filter_offset + delta);
                }
                if (col.__isset.offset_index_offset)
                    col.__set_offset_index_offset(col.offset_index_offset + delta);
                if (col.__isset.column_index_offset)
                    col.__set_column_index_offset(col.column_index_offset + delta);
            }
            if (rg.__isset.file_offset)
                rg.__set_file_offset(rg.file_offset + delta);
            merged_metadata.num_rows += rg.num_rows;
            merged_metadata.row_groups.push_back(std::move(rg));
        }

        if (row_group_data_size > 0)
        {
            const auto ws = sink->Write(data + 4, row_group_data_size);
            if (!ws.ok())
            {
                pipeline_error = ws.ToString();
                return;
            }
            current_output_data_offset += row_group_data_size;
        }
        AddCommittedRows(ctx, file_meta.num_rows);
        MarkPartCompleted(ctx);
    };

    std::unordered_map<std::int32_t, InMemoryPartitionData> pending;
    std::int32_t expected_part = 1;

    InMemoryPartitionData result;
    while (result_queue.Pop(&result))
    {
        pending[result.part] = std::move(result);

        while (expected_part <= part_count && pipeline_error.empty())
        {
            auto it = pending.find(expected_part);
            if (it == pending.end())
                break;

            if (!it->second.error.empty())
                pipeline_error = "partition " + std::to_string(expected_part) + " failed: " + it->second.error;
            else if (it->second.buffer)
                process_buffer(it->second.buffer, expected_part);

            pending.erase(it);
            ++expected_part;
        }

        if (!pipeline_error.empty())
        {
            stop_requested.store(true, std::memory_order_release);
            partition_queue.Close();
            result_queue.Close();
            break;
        }
    }

    for (auto & future : worker_futures)
    {
        try
        {
            (void)std::move(future).Get().Ok();
        }
        catch (...)
        {
            if (pipeline_error.empty())
                pipeline_error = "worker task threw exception";
        }
    }

    if (!pipeline_error.empty())
    {
        (void)sink->Abort();
        (void)fs->DeleteFile(temp_path);
        return fail(pipeline_error);
    }

    if (expected_part != part_count + 1)
        return fail("not all partitions completed");

    // Serialize merged Parquet footer and write the file trailer.
    ::parquet::ThriftSerializer serializer;
    std::uint32_t serialized_len = 0;
    std::uint8_t * serialized_buf = nullptr;
    serializer.SerializeToBuffer(&merged_metadata, &serialized_len, &serialized_buf);

    {
        const auto ws = sink->Write(serialized_buf, serialized_len);
        if (!ws.ok())
            return fail(ws.ToString());
    }
    {
        const std::uint8_t footer_len_bytes[4] = {
            static_cast<std::uint8_t>(serialized_len & 0xFF),
            static_cast<std::uint8_t>((serialized_len >> 8) & 0xFF),
            static_cast<std::uint8_t>((serialized_len >> 16) & 0xFF),
            static_cast<std::uint8_t>((serialized_len >> 24) & 0xFF)};
        const auto ws = sink->Write(footer_len_bytes, 4);
        if (!ws.ok())
            return fail(ws.ToString());
    }
    {
        const auto ws = sink->Write(kParquetMagic, 4);
        if (!ws.ok())
            return fail(ws.ToString());
    }

    const auto close_status = sink->Close();
    if (!close_status.ok())
        return fail(close_status.ToString());

    const auto move_status = fs->Move(temp_path, final_path);
    if (!move_status.ok())
        return fail(move_status.ToString());

    return 0;
}
#endif

int RunNativeMultiplexedOrdered(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    std::int32_t part_count,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
#if defined(ARROW_PARQUET)
    if (writer_options.format == OutputFormat::Parquet)
    {
        const auto & parquet_opts = ParquetTableWriter::ResolveOptions(writer_options);
        return RunParallelEncodeParquetOrdered(ctx, table, output, parquet_opts, part_count, scheduler_options, part_executor);
    }
#endif
    auto writer = format_driver.CreateOrderedWriter();
    std::unique_ptr<ISingleFileSink> sink;
    const bool use_sink_fallback = writer == nullptr;

    if (use_sink_fallback)
    {
        auto sink_result = OpenSingleFileSink(table, output, writer_options, ctx.pool);
        if (!sink_result.ok())
        {
            std::ostringstream message;
            message << "Failed to open single output file for table " << table.name << " (format " << format_driver.Name()
                    << "): " << sink_result.status().ToString();
            ReportError(ctx, message.str());
            return 2;
        }
        sink = std::move(sink_result).ValueOrDie();
    }
    else
    {
        const auto open_status = writer->OpenTable(table, output, writer_options, ctx.pool);
        if (!open_status.ok())
        {
            ReportError(ctx, "Failed to open ordered output for table " + table.name + ": " + open_status.ToString());
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
                    const auto row_count = state.batches.front().row_count;
                    if (use_sink_fallback)
                    {
                        RETURN_NOT_OK(sink->WriteBatch(state.batches.front()));
                    }
                    else
                    {
                        RETURN_NOT_OK(writer->WriteBatch(state.batches.front()));
                    }
                    state.batches.pop_front();
                    AddCommittedRows(ctx, row_count);
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

            MarkPartCompleted(ctx);
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
        ReportError(ctx, "single-file-ordered write failed for table " + table.name + ": " + pipeline_error);
        return 2;
    }
    return 0;
}

int RunForeignStreamingOrdered(
    const GenerationContext & ctx,
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & writer_options,
    const IFormatDriver & format_driver,
    std::int32_t part_count,
    const WriteSchedulerOptions & scheduler_options,
    yaclib::IExecutor & part_executor)
{
    (void)scheduler_options;
    (void)part_executor;

    auto writer = format_driver.CreateOrderedWriter();
    if (writer == nullptr)
    {
        ReportError(ctx, "Ordered writer is not available for format " + std::string(format_driver.Name()));
        return 2;
    }

    bool table_open = false;

    auto close_writer = [&]() -> arrow::Status {
        if (!table_open)
        {
            return arrow::Status::OK();
        }

        table_open = false;
        return writer->CloseTable();
    };

    auto fail = [&](const std::string & message) -> int {
        ReportError(ctx, "single-file-ordered write failed for table " + table.name + ": " + message);
        const auto close_status = close_writer();
        if (!close_status.ok())
        {
            ReportError(ctx, "single-file-ordered cleanup failed for table " + table.name + ": " + close_status.ToString());
        }
        return 2;
    };

    try
    {
        const auto open_status = writer->OpenTable(table, output, writer_options, ctx.pool);
        if (!open_status.ok())
        {
            ReportError(ctx, "Failed to open ordered output for table " + table.name + ": " + open_status.ToString());
            return 2;
        }
        table_open = true;

        for (std::int32_t part = 1; part <= part_count; ++part)
        {
            const PartitionSpec partition{.part_num = part, .part_count = part_count};
            const auto begin_status = writer->BeginInputPartition(partition);
            if (!begin_status.ok())
            {
                return fail(begin_status.ToString());
            }

            auto generator = ctx.registry->CreateGenerator(table.name, ctx.pool);
            if (!generator)
            {
                return fail("Failed to create generator for table: " + table.name);
            }

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
                    return fail(write_status.ToString());
                }
                AddCommittedRows(ctx, batch.row_count);
            }

            const auto end_status = writer->EndInputPartition();
            if (!end_status.ok())
            {
                return fail(end_status.ToString());
            }
            MarkPartCompleted(ctx);
        }
    }
    catch (const std::exception & ex)
    {
        return fail(ex.what());
    }
    catch (...)
    {
        return fail("Unknown partition generation error");
    }

    const auto close_status = close_writer();
    if (!close_status.ok())
    {
        ReportError(ctx, "single-file-ordered write failed for table " + table.name + ": " + close_status.ToString());
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
        ReportError(ctx, "Invalid part_count for table " + table.name);
        return 2;
    }

    auto selected_strategy_result = ResolveSelectedWriteStrategy(scheduler_options, format_driver);
    if (!selected_strategy_result.ok())
    {
        ReportError(ctx, selected_strategy_result.status().ToString());
        return 2;
    }
    const auto strategy = std::move(selected_strategy_result).ValueOrDie();
    MarkTableStarted(ctx, part_count);

    int rc = 2;
    switch (strategy)
    {
        case WriteStrategy::ParallelPartitionFiles:
            rc = RunParallelPartitionFiles(
                ctx, table, output, writer_options, format_driver, part_count, scheduler_options, part_executor);
            break;
        case WriteStrategy::SingleFileOrdered:
            switch (format_driver.OrderedExecutionModel())
            {
                case OrderedWriteExecutionModel::NativeMultiplexed:
                    rc = RunNativeMultiplexedOrdered(
                        ctx, table, output, writer_options, format_driver, part_count, scheduler_options, part_executor);
                    break;
                case OrderedWriteExecutionModel::ForeignStreaming:
                    rc = RunForeignStreamingOrdered(
                        ctx, table, output, writer_options, format_driver, part_count, scheduler_options, part_executor);
                    break;
                default:
                    ReportError(ctx, "Unsupported ordered execution model for format " + std::string(format_driver.Name()));
                    return 2;
            }
            break;
        default:
            return 2;
    }

    if (rc == 0)
    {
        MarkTableFinished(ctx);
    }
    return rc;
}
