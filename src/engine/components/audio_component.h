#pragma once

#include <limits>
#include <string>

#include "component.h"
#include "engine/core/systems/audio/audio_clip.h"

struct MIX_Track;

namespace hob {
    using AudioIndex = uint32_t;
    constexpr AudioIndex INVALID_AUDIO_INDEX = std::numeric_limits<AudioIndex>::max();

    class AudioComponent : public Component {
        friend class EntitySpawner;

        AudioClipRef m_clip;
        float m_volume = 1.0f;
        bool m_loop = false;
        bool m_spatial = false;
        float m_max_distance = 20.0f;
        bool m_autoplay = false;

        MIX_Track* m_track = nullptr;

        // Slot in EntitySpawner's audio registry.
        AudioIndex m_audio_index = INVALID_AUDIO_INDEX;

    public:
        explicit AudioComponent(Entity& entity);

        void enter_play() override;
        void exit_play() override;

        std::string to_string() const override;

        void play();
        void stop();
        bool is_playing() const;

        const AudioClipRef& get_clip() const;
        void set_clip(AudioClipRef clip);

        float get_volume() const;
        void set_volume(float volume);

        bool is_looping() const;
        void set_looping(bool looping);

        bool is_spatial() const;
        void set_spatial(bool spatial);

        float get_max_distance() const;
        void set_max_distance(float max_distance);

        bool get_autoplay() const;
        void set_autoplay(bool autoplay);

        // Recompute this source's listener-relative 3D position. Driven by Audio::tick so it runs
        // regardless of whether the owning entity is ticking.
        void update_spatialization();
    };
} // namespace hob
