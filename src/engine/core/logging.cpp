#include "logging.h"

#include <iostream>
#include <mutex>

namespace hob::log::detail {
    namespace {
        std::mutex g_log_mutex;
    } // namespace

    void write_out(std::string_view message) {
        std::lock_guard lock(g_log_mutex);
        std::cout << message << std::endl;
    }

    void write_err(std::string_view message) {
        std::lock_guard lock(g_log_mutex);
        std::cerr << message << std::endl;
    }

    void write_out(std::string_view tag, std::string_view message) {
        std::lock_guard lock(g_log_mutex);
        std::cout << '[' << tag << "] " << message << std::endl;
    }

    void write_err(std::string_view tag, std::string_view message) {
        std::lock_guard lock(g_log_mutex);
        std::cerr << '[' << tag << "] " << message << std::endl;
    }
} // namespace hob::log::detail
