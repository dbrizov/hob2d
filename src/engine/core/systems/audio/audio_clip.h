#pragma once

#include <memory>
#include <string>

struct MIX_Audio;

namespace hob {
    class Audio;

    class AudioClip;
    using AudioClipRef = std::shared_ptr<AudioClip>;
    using AudioClipWeakRef = std::weak_ptr<AudioClip>;

    class AudioClip {
        friend class Audio;

        MIX_Audio* m_handle = nullptr;
        std::string m_path;

        AudioClip(MIX_Audio* handle, std::string relative_path);

    public:
        ~AudioClip();

        AudioClip(const AudioClip&) = delete;
        AudioClip& operator=(const AudioClip&) = delete;

        AudioClip(AudioClip&&) = delete;
        AudioClip& operator=(AudioClip&&) = delete;

        MIX_Audio* get_handle() const;
        const std::string& get_path() const;
    };
} // namespace hob
