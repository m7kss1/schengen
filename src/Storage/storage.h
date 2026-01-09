#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <arrow/filesystem/filesystem.h>
#include <arrow/filesystem/localfs.h>
#include <arrow/result.h>
#include <arrow/status.h>

using FileSystemPtr = std::shared_ptr<arrow::fs::FileSystem>;

inline bool IsObsUri(std::string_view uri)
{
    return uri.rfind("obs://", 0) == 0;
}

inline bool HasUriScheme(std::string_view uri)
{
    return uri.find("://") != std::string_view::npos || uri.rfind("s3:", 0) == 0;
}

inline arrow::Result<FileSystemPtr> ResolveTarget(std::string_view uri)
{
    FileSystemPtr option;

    /* TODO: OBS support via: https://github.com/huaweicloud/huaweicloud-sdk-c-obs */
    if (IsObsUri(uri))
    {
        return arrow::Status::NotImplemented("Direct writes into OBS filesystem is not implemented yet");
    }

    if (HasUriScheme(uri))
    {
        ARROW_ASSIGN_OR_RAISE(option, arrow::fs::FileSystemFromUri(std::string(uri)));
    }
    else
    {
        option = std::make_shared<arrow::fs::LocalFileSystem>();
    }

    return option;
}
