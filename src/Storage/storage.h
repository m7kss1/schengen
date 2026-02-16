#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <arrow/filesystem/filesystem.h>
#include <arrow/filesystem/localfs.h>
#include <arrow/result.h>
#include <arrow/status.h>

using FileSystemPtr = std::shared_ptr<arrow::fs::FileSystem>;

bool IsObsUri(std::string_view uri);

bool HasUriScheme(std::string_view uri);

arrow::Result<FileSystemPtr> ResolveTarget(std::string_view uri);
