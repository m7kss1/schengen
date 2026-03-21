#pragma once

#include <arrow/c/abi.h>

#include <cstdint>

extern "C" {

struct VortexWriterHandle;

struct VortexWriterOptionsC
{
    std::uint32_t abi_version;
    std::uint32_t reserved;
    std::int64_t row_block_size;
    std::int64_t output_buffer_bytes;
};

struct VortexFileInfoC
{
    std::int64_t row_count;
    char * field_names_csv;
};

int vortex_writer_open(
    const char * uri,
    ArrowSchema * schema,
    const VortexWriterOptionsC * options,
    VortexWriterHandle ** out_handle);
int vortex_writer_begin_partition(VortexWriterHandle * handle, std::int32_t part_num, std::int32_t part_count);
int vortex_writer_push_batch(VortexWriterHandle * handle, ArrowArray * batch);
int vortex_writer_end_partition(VortexWriterHandle * handle);
int vortex_writer_finish(VortexWriterHandle * handle);
void vortex_writer_destroy(VortexWriterHandle * handle);
const char * vortex_last_error(const VortexWriterHandle * handle);
const char * vortex_bridge_last_error();

int vortex_file_inspect(const char * uri, VortexFileInfoC * out_info);
void vortex_file_info_destroy(VortexFileInfoC * info);

} // extern "C"
