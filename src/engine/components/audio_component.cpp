#include "audio_component.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "engine/components/camera_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/audio/audio.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/entity/entity.h"
#include "engine/math/vector2.h"

namespace hob {
    AudioComponent::AudioComponent(Entity& entity)
        : Component(entity) {}

    void AudioComponent::enter_play() {
        m_track = get_engine().get_audio().create_track();
        get_engine().get_entity_spawner().register_audio(this);

        if (m_autoplay && m_clip) {
            play();
        }
    }

    void AudioComponent::exit_play() {
        get_engine().get_entity_spawner().unregister_audio(this);
        get_engine().get_audio().destroy_track(m_track);
        m_track = nullptr;
    }

    std::string AudioComponent::to_string() const {
        return "AudioComponent";
    }

    void AudioComponent::play() {
        Audio& audio = get_engine().get_audio();
        audio.play_track(m_track, m_clip, m_volume, m_loop);

        // play_track sets the base gain; refine it (and the pan) for spatial sources.
        if (m_spatial) {
            update_spatialization();
        }
        else {
            audio.reset_track_pan(m_track);
        }
    }

    void AudioComponent::stop() {
        get_engine().get_audio().stop_track(m_track);
    }

    bool AudioComponent::is_playing() const {
        return get_engine().get_audio().is_track_playing(m_track);
    }

    const AudioClipRef& AudioComponent::get_clip() const {
        return m_clip;
    }

    void AudioComponent::set_clip(AudioClipRef clip) {
        m_clip = std::move(clip);
    }

    float AudioComponent::get_volume() const {
        return m_volume;
    }

    void AudioComponent::set_volume(float volume) {
        m_volume = volume;
        apply_gain();
    }

    bool AudioComponent::is_looping() const {
        return m_loop;
    }

    void AudioComponent::set_looping(bool looping) {
        m_loop = looping;
    }

    bool AudioComponent::is_spatial() const {
        return m_spatial;
    }

    void AudioComponent::set_spatial(bool spatial) {
        m_spatial = spatial;
        if (!m_spatial) {
            // Drop back to plain, centered playback; apply_gain restores the base volume.
            get_engine().get_audio().reset_track_pan(m_track);
        }
        apply_gain();
    }

    float AudioComponent::get_max_distance() const {
        return m_max_distance;
    }

    void AudioComponent::set_max_distance(float max_distance) {
        m_max_distance = max_distance;
    }

    bool AudioComponent::get_autoplay() const {
        return m_autoplay;
    }

    void AudioComponent::set_autoplay(bool autoplay) {
        m_autoplay = autoplay;
    }

    void AudioComponent::update_spatialization() {
        if (!m_spatial || !is_playing()) {
            return;
        }

        CameraComponent* camera = get_engine().get_active_camera();
        if (!camera) {
            return;
        }

        const Vector2 listener = camera->get_entity().get_transform()->get_position();
        const Vector2 emitter = get_entity().get_transform()->get_position();
        const Vector2 relative = emitter - listener;
        const float distance = relative.length();

        Audio& audio = get_engine().get_audio();

        // Linear distance falloff: full volume at the listener, silent at (and beyond) max_distance.
        const float falloff =
            (m_max_distance > 0.0f) ? std::clamp(1.0f - (distance / m_max_distance), 0.0f, 1.0f) : 1.0f;
        audio.set_track_gain(m_track, m_volume * falloff);

        // Pan by horizontal direction (independent of distance): -1 = left, +1 = right.
        const float pan = (distance > EPSILON) ? std::clamp(relative.x / distance, -1.0f, 1.0f) : 0.0f;
        audio.set_track_pan(m_track, pan);
    }

    void AudioComponent::apply_gain() {
        if (m_spatial) {
            update_spatialization(); // derives base*falloff (+pan); no-op when not playing
        }
        else {
            get_engine().get_audio().set_track_gain(m_track, m_volume);
        }
    }
} // namespace hob
