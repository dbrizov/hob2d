#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio_clip.h"

struct MIX_Mixer;
struct MIX_Track;

namespace hob {
    struct AudioConfig;

    class Audio {
        bool m_enabled;
        MIX_Mixer* m_mixer = nullptr;

        std::unordered_map<std::string, AudioClipWeakRef> m_clips;
        std::vector<MIX_Track*> m_oneshot_tracks;

    public:
        explicit Audio(const AudioConfig& audio_config);
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        Audio(Audio&&) = delete;
        Audio& operator=(Audio&&) = delete;

        bool is_enabled() const;
        MIX_Mixer* get_mixer() const;

        AudioClipRef get_or_load_clip(const std::string& relative_path);

        void play_oneshot(const AudioClipRef& clip, float volume = 1.0f);

        float get_master_volume() const;
        void set_master_volume(float volume);

    private:
        MIX_Track* acquire_oneshot_track();
    };
} // namespace hob
