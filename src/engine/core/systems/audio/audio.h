#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio_clip.h"

struct MIX_Mixer;
struct MIX_Track;

namespace hob {
    struct AudioConfig;
    class Console;

    class Audio {
        bool m_enabled;
        MIX_Mixer* m_mixer = nullptr;

        std::unordered_map<std::string, AudioClipWeakRef> m_clips;
        std::vector<MIX_Track*> m_oneshot_tracks;

        bool m_cvar_show_clips = false;

    public:
        explicit Audio(const AudioConfig& audio_config);
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        Audio(Audio&&) = delete;
        Audio& operator=(Audio&&) = delete;

        void register_cvars(Console& console);
        void debug_clips();

        bool is_enabled() const;
        void set_enabled(bool enabled);

        MIX_Mixer* get_mixer() const;

        AudioClipRef get_or_load_clip(const std::string& relative_path);

        void play_oneshot(const AudioClipRef& clip, float volume = 1.0f);

        float get_master_volume() const;
        void set_master_volume(float volume);

        MIX_Track* create_track();
        void destroy_track(MIX_Track* track);
        void play_track(MIX_Track* track, const AudioClipRef& clip, float volume, bool loop);
        void stop_track(MIX_Track* track);
        bool is_track_playing(MIX_Track* track) const;
        void set_track_gain(MIX_Track* track, float volume);
        void set_track_pan(MIX_Track* track, float pan); // -1 = full left, 0 = center, +1 = full right
        void reset_track_pan(MIX_Track* track);

    private:
        MIX_Track* acquire_oneshot_track();
    };
} // namespace hob
