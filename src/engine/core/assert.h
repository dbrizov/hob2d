#pragma once

#include <cstdlib>
#include <format>
#include <string_view>

#include "engine/core/logging.h"

namespace hob::detail {
    [[noreturn]] inline void report_fatal(std::string_view message, const char* file, int line) {
        log::engine.error("FATAL: {} ({}:{})", message, file, line);
        std::abort();
    }
} // namespace hob::detail

#define HOB_CHECK(cond, ...)                                                                                           \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            ::hob::detail::report_fatal(::std::format(__VA_ARGS__), __FILE__, __LINE__);                               \
        }                                                                                                              \
    }                                                                                                                  \
    while (false)
