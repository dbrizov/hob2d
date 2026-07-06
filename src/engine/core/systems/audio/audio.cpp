#include "audio.h"

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
        if (handle == nullptr) {
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
        if (track == nullptr) {
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

    MIX_Track* Audio::acquire_oneshot_track() {
        // Reclaim a track that has finished playing.
        for (MIX_Track* track : m_oneshot_tracks) {
            if (!MIX_TrackPlaying(track)) {
                return track;
            }
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (track == nullptr) {
            log::audio.error("MIX_CreateTrack failed: {}", SDL_GetError());
            return nullptr;
        }

        m_oneshot_tracks.push_back(track);
        return track;
    }
} // namespace hob
