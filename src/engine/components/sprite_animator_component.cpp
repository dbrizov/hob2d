#include "sprite_animator_component.h"

#include <cmath>
#include <utility>

#include "engine/entity/entity.h"

namespace hob {
    SpriteAnimatorComponent::SpriteAnimatorComponent(Entity& entity)
        : Component(entity) {}

    void SpriteAnimatorComponent::enter_play() {
        if (!m_default_clip_name.empty() && m_clips.contains(m_default_clip_name)) {
            play(m_default_clip_name);
        }
    }

    void SpriteAnimatorComponent::tick(float delta_time) {
        if (!m_playing || m_current_clip == nullptr) {
            return;
        }

        const float duration = m_current_clip->duration;
        m_time += delta_time;

        if (m_time >= duration) {
            if (m_current_clip->looping) {
                m_time = duration > 0.0f ? std::fmod(m_time, duration) : 0.0f;
            }
            else {
                m_time = duration;
                m_playing = false;
            }
        }

        apply_key_values();
    }

    void SpriteAnimatorComponent::apply_key_values() {
        if (m_current_clip == nullptr) {
            return;
        }

        Entity& entity = get_entity();
        for (const AnimationTrackRef& track : m_current_clip->tracks) {
            track->apply_key_values(entity, m_time);
        }
    }

    std::string SpriteAnimatorComponent::to_string() const {
        return "SpriteAnimatorComponent";
    }

    void SpriteAnimatorComponent::add_clip(const std::string& name, AnimationClipRef clip) {
        m_clips[name] = std::move(clip);
    }

    const AnimationClips& SpriteAnimatorComponent::get_clips() const {
        return m_clips;
    }

    void SpriteAnimatorComponent::set_clips(AnimationClips clips) {
        m_clips = std::move(clips);

        // Re-point the active clip at its replacement so hot-reloaded clip data (new tracks/keyframes)
        // takes effect on the currently-playing clip, not just the next play(). Keeps the time cursor.
        if (!m_current_clip_name.empty()) {
            auto it = m_clips.find(m_current_clip_name);
            m_current_clip = it != m_clips.end() ? it->second : nullptr;
            if (m_current_clip == nullptr) {
                m_playing = false;
            }
        }
    }

    void SpriteAnimatorComponent::clear_clips() {
        m_clips.clear();
    }

    void SpriteAnimatorComponent::set_default_clip(const std::string& name) {
        m_default_clip_name = name;
    }

    const std::string& SpriteAnimatorComponent::get_default_clip() const {
        return m_default_clip_name;
    }

    const std::string& SpriteAnimatorComponent::get_current_clip() const {
        return m_current_clip_name;
    }

    void SpriteAnimatorComponent::play(const std::string& name) {
        auto it = m_clips.find(name);
        if (it == m_clips.end()) {
            return;
        }

        m_current_clip = it->second;
        m_current_clip_name = name;
        m_time = 0.0f;
        m_playing = true;

        // Show the first frame / pose immediately.
        apply_key_values();
    }

    void SpriteAnimatorComponent::resume() {
        if (m_current_clip != nullptr) {
            m_playing = true;
        }
    }

    void SpriteAnimatorComponent::pause() {
        m_playing = false;
    }

    void SpriteAnimatorComponent::stop() {
        m_playing = false;
        m_time = 0.0f;
    }

    bool SpriteAnimatorComponent::is_playing() const {
        return m_playing;
    }
} // namespace hob
