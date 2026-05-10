#include "Storage/storage.h"

#include <arrow/util/config.h>

#include <mutex>

#if defined(ARROW_S3)
#    include <arrow/filesystem/s3fs.h>
#endif

namespace
{
bool IsS3Uri(std::string_view uri)
{
    return uri.rfind("s3://", 0) == 0 || uri == "s3:" || uri.rfind("s3:", 0) == 0;
}

std::mutex s3_mutex;
bool s3_initialized = false;
bool s3_finalized = false;
arrow::Status s3_init_status = arrow::Status::OK();

arrow::Status EnsureS3Initialized(std::string_view uri)
{
    if (!IsS3Uri(uri))
    {
        return arrow::Status::OK();
    }

#if defined(ARROW_S3)
    std::lock_guard lock(s3_mutex);
    if (s3_finalized)
    {
        return arrow::Status::Invalid("S3 filesystem was already finalized");
    }
    if (!s3_initialized && s3_init_status.ok())
    {
        s3_init_status = arrow::fs::InitializeS3(arrow::fs::S3GlobalOptions::Defaults());
        s3_initialized = s3_init_status.ok();
    }
    return s3_init_status;
#else
    return arrow::Status::NotImplemented("Arrow was built without S3 filesystem support");
#endif
}
}

bool IsObsUri(std::string_view uri)
{
    return uri.rfind("obs://", 0) == 0;
}

bool HasUriScheme(std::string_view uri)
{
    return uri.find("://") != std::string_view::npos || uri.rfind("s3:", 0) == 0;
}

arrow::Result<FileSystemPtr> ResolveTarget(std::string_view uri)
{
    FileSystemPtr option;

    /* TODO: OBS support via: https://github.com/huaweicloud/huaweicloud-sdk-c-obs */
    if (IsObsUri(uri))
    {
        return arrow::Status::NotImplemented("Direct writes into OBS filesystem is not implemented yet");
    }

    if (HasUriScheme(uri))
    {
        RETURN_NOT_OK(EnsureS3Initialized(uri));
        ARROW_ASSIGN_OR_RAISE(option, arrow::fs::FileSystemFromUri(std::string(uri)));
    }
    else
    {
        option = std::make_shared<arrow::fs::LocalFileSystem>();
    }

    return option;
}

arrow::Status FinalizeStorageBackends()
{
#if defined(ARROW_S3)
    std::lock_guard lock(s3_mutex);
    if (!s3_initialized || s3_finalized)
    {
        return arrow::Status::OK();
    }
    s3_finalized = true;
    return arrow::fs::FinalizeS3();
#else
    return arrow::Status::OK();
#endif
}
