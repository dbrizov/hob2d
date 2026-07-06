#include "audio_clip.h"

#include <utility>

#include <SDL3_mixer/SDL_mixer.h>

namespace hob {
    AudioClip::AudioClip(MIX_Audio* handle, std::string relative_path)
        : m_handle(handle)
        , m_path(std::move(relative_path)) {}

    AudioClip::~AudioClip() {
        if (m_handle != nullptr) {
            MIX_DestroyAudio(m_handle);
            m_handle = nullptr;
        }
    }

    MIX_Audio* AudioClip::get_handle() const {
        return m_handle;
    }

    const std::string& AudioClip::get_path() const {
        return m_path;
    }
} // namespace hob
