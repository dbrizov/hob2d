#include "ui_file_interface.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include "engine/core/path_utils.h"

namespace hob {
    Rml::FileHandle UiFileInterface::Open(const Rml::String& path) {
        const std::filesystem::path full = PathUtils::resolve_asset_path(path);

        auto stream = std::make_unique<std::ifstream>(full, std::ios::binary);
        if (!stream->is_open()) {
            return 0;
        }

        return reinterpret_cast<Rml::FileHandle>(stream.release());
    }

    void UiFileInterface::Close(Rml::FileHandle file) {
        delete reinterpret_cast<std::ifstream*>(file);
    }

    size_t UiFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file) {
        auto* stream = reinterpret_cast<std::ifstream*>(file);
        stream->read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));

        return static_cast<size_t>(stream->gcount());
    }

    bool UiFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {
        auto* stream = reinterpret_cast<std::ifstream*>(file);

        std::ios_base::seekdir dir = std::ios_base::beg;
        if (origin == SEEK_CUR) {
            dir = std::ios_base::cur;
        }
        else if (origin == SEEK_END) {
            dir = std::ios_base::end;
        }

        stream->clear();
        stream->seekg(offset, dir);
        return !stream->fail();
    }

    size_t UiFileInterface::Tell(Rml::FileHandle file) {
        auto* stream = reinterpret_cast<std::ifstream*>(file);
        return stream->tellg();
    }
} // namespace hob
