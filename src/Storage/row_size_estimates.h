#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

inline const std::unordered_map<std::string_view, std::int64_t> g_approxRowBytesTPCH = {
    {"nation", 117},
    {"region", 151},
    {"part", 70},
    {"supplier", 164},
    {"partsupp", 141 * 4},
    {"customer", 168},
    {"orders", 75},
    {"lineitem", 64},
};

inline std::int64_t EstimateTpchRowBytes(std::string_view table_name)
{
    const auto it = g_approxRowBytesTPCH.find(table_name);
    return it == g_approxRowBytesTPCH.end() ? 0 : it->second;
}
