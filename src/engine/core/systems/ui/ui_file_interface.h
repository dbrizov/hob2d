#pragma once

#include <RmlUi/Core/FileInterface.h>

namespace hob {
    class UiFileInterface : public Rml::FileInterface {
    public:
        Rml::FileHandle Open(const Rml::String& relative_path) override;
        void Close(Rml::FileHandle file) override;
        size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
        bool Seek(Rml::FileHandle file, long offset, int origin) override;
        size_t Tell(Rml::FileHandle file) override;
    };
} // namespace hob
