#include "logging.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>

#include "path_utils.h"

namespace hob::log::detail {
    namespace {
        std::mutex g_log_mutex;

        // Opened once on first use and truncated, so each run starts a fresh log file.
        std::ofstream& log_file() {
            static std::ofstream file(PathUtils::get_log_path(), std::ios::out | std::ios::trunc);
            return file;
        }

        void emit(std::ostream& console, std::string_view tag, std::string_view message) {
            std::lock_guard lock(g_log_mutex);
            std::ofstream& file = log_file();
            if (tag.empty()) {
                console << message << std::endl;
                if (file) {
                    file << message << std::endl;
                }
            }
            else {
                console << '[' << tag << "] " << message << std::endl;
                if (file) {
                    file << '[' << tag << "] " << message << std::endl;
                }
            }
        }
    } // namespace

    void write_out(std::string_view message) {
        emit(std::cout, {}, message);
    }

    void write_err(std::string_view message) {
        emit(std::cerr, {}, message);
    }

    void write_out(std::string_view tag, std::string_view message) {
        emit(std::cout, tag, message);
    }

    void write_err(std::string_view tag, std::string_view message) {
        emit(std::cerr, tag, message);
    }
} // namespace hob::log::detail
