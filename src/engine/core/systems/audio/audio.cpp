#include "audio.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "engine/core/assert.h"
#include "engine/core/engine_config.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"

namespace hob {
    Audio::Audio(const AudioConfig& audio_config)
        : m_enabled(audio_config.enabled) {
        if (!m_enabled) {
            log::audio.info("Audio disabled by config");
            return;
        }

        const bool mixer_initialized = MIX_Init();
        HOB_CHECK(mixer_initialized, "MIX_Init failed: {}", SDL_GetError());

        m_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        HOB_CHECK(m_mixer, "MIX_CreateMixerDevice failed: {}", SDL_GetError());

        MIX_SetMixerGain(m_mixer, audio_config.master_volume);
        log::audio.info("Audio initialized (master volume {})", audio_config.master_volume);
    }

    Audio::~Audio() {
        for (MIX_Track* track : m_oneshot_tracks) {
            MIX_DestroyTrack(track);
        }
        m_oneshot_tracks.clear();

        if (m_mixer != nullptr) {
            MIX_DestroyMixer(m_mixer);
            m_mixer = nullptr;
            MIX_Quit();
            log::audio.info("Audio shut down");
        }
    }

    bool Audio::is_enabled() const {
        return m_enabled;
    }

    void Audio::set_enabled(bool enabled) {
        m_enabled = enabled;
    }

    MIX_Mixer* Audio::get_mixer() const {
        return m_mixer;
    }

    AudioClipRef Audio::get_or_load_clip(const std::string& relative_path) {
        if (!m_enabled) {
            return nullptr;
        }

        const std::string key = std::filesystem::path(relative_path).lexically_normal().string();

        auto it = m_clips.find(key);
        if (it != m_clips.end()) {
            if (AudioClipRef cached = it->second.lock()) {
                return cached;
            }
        }

        const std::filesystem::path full_path = PathUtils::resolve_asset_path(relative_path);
        MIX_Audio* handle = MIX_LoadAudio(m_mixer, full_path.string().c_str(), true);
        if (!handle) {
            log::audio.error("MIX_LoadAudio failed for '{}': {}", key, SDL_GetError());
            return nullptr;
        }

        AudioClipRef clip(new AudioClip(handle, key));
        m_clips[key] = clip;
        return clip;
    }

    void Audio::play_oneshot(const AudioClipRef& clip, float volume) {
        if (!m_enabled || !clip) {
            return;
        }

        MIX_Track* track = acquire_oneshot_track();
        if (!track) {
            return;
        }

        MIX_SetTrackAudio(track, clip->get_handle());
        MIX_SetTrackGain(track, volume);
        if (!MIX_PlayTrack(track, 0)) {
            log::audio.error("MIX_PlayTrack failed for '{}': {}", clip->get_path(), SDL_GetError());
        }
    }

    float Audio::get_master_volume() const {
        if (!m_enabled) {
            return 0.0f;
        }

        return MIX_GetMixerGain(m_mixer);
    }

    void Audio::set_master_volume(float volume) {
        if (!m_enabled) {
            return;
        }

        MIX_SetMixerGain(m_mixer, volume);
    }

    MIX_Track* Audio::create_track() {
        if (!m_enabled) {
            return nullptr;
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (!track) {
            log::audio.error("MIX_CreateTrack failed: {}", SDL_GetError());
        }

        return track;
    }

    void Audio::destroy_track(MIX_Track* track) {
        if (track) {
            MIX_DestroyTrack(track);
        }
    }

    void Audio::play_track(MIX_Track* track, const AudioClipRef& clip, float volume, bool loop) {
        if (!m_enabled || !track || !clip) {
            return;
        }

        MIX_SetTrackAudio(track, clip->get_handle());
        MIX_SetTrackGain(track, volume);

        SDL_PropertiesID options = 0;
        if (loop) {
            options = SDL_CreateProperties();
            SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
        }

        if (!MIX_PlayTrack(track, options)) {
            log::audio.error("MIX_PlayTrack failed for '{}': {}", clip->get_path(), SDL_GetError());
        }

        if (options != 0) {
            SDL_DestroyProperties(options);
        }
    }

    void Audio::stop_track(MIX_Track* track) {
        if (!m_enabled || !track) {
            return;
        }

        MIX_StopTrack(track, 0);
    }

    bool Audio::is_track_playing(MIX_Track* track) const {
        if (!m_enabled || !track) {
            return false;
        }

        return MIX_TrackPlaying(track);
    }

    void Audio::set_track_gain(MIX_Track* track, float volume) {
        if (!m_enabled || !track) {
            return;
        }

        MIX_SetTrackGain(track, volume);
    }

    void Audio::set_track_pan(MIX_Track* track, float pan) {
        if (!m_enabled || !track) {
            return;
        }

        // Constant-power pan: left^2 + right^2 == 1, so a centered source isn't louder than a hard-panned one.
        const float t = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.5f; // 0 = full left, 1 = full right
        MIX_StereoGains gains;
        gains.left = std::sqrt(1.0f - t);
        gains.right = std::sqrt(t);
        MIX_SetTrackStereo(track, &gains);
    }

    void Audio::clear_track_pan(MIX_Track* track) {
        if (!m_enabled || !track) {
            return;
        }

        MIX_SetTrackStereo(track, nullptr);
    }

    MIX_Track* Audio::acquire_oneshot_track() {
        // Reclaim a track that has finished playing.
        for (MIX_Track* track : m_oneshot_tracks) {
            if (!MIX_TrackPlaying(track)) {
                return track;
            }
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (!track) {
            log::audio.error("MIX_CreateTrack failed: {}", SDL_GetError());
            return nullptr;
        }

        m_oneshot_tracks.push_back(track);
        return track;
    }
} // namespace hob
