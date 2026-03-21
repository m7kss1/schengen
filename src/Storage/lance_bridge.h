#pragma once

#include <arrow/c/abi.h>

#include <cstdint>

extern "C" {

struct LanceWriterHandle;

struct LanceWriterOptionsC
{
    std::int64_t max_rows_per_file;
    std::int64_t max_rows_per_group;
    std::int64_t max_bytes_per_file;
};

struct LanceDatasetInfoC
{
    std::int64_t row_count;
    char * field_names_csv;
};

int lance_writer_open(
    const char * uri,
    ArrowSchema * schema,
    const LanceWriterOptionsC * options,
    LanceWriterHandle ** out_handle);
int lance_writer_begin_partition(LanceWriterHandle * handle, std::int32_t part_num, std::int32_t part_count);
int lance_writer_push_batch(LanceWriterHandle * handle, ArrowArray * batch);
int lance_writer_end_partition(LanceWriterHandle * handle);
int lance_writer_finish(LanceWriterHandle * handle);
void lance_writer_destroy(LanceWriterHandle * handle);
const char * lance_last_error(const LanceWriterHandle * handle);
const char * lance_bridge_last_error();

int lance_dataset_inspect(const char * uri, LanceDatasetInfoC * out_info);
void lance_dataset_info_destroy(LanceDatasetInfoC * info);

} // extern "C"
