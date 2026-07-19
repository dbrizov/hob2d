#pragma once

#include <string>
#include <unordered_map>

#include "component.h"
#include "engine/animation/animation_clip.h"

namespace hob {
    using AnimationClips = std::unordered_map<std::string, AnimationClipRef>;

    class SpriteAnimatorComponent : public Component {
        AnimationClips m_clips;
        AnimationClipRef m_current_clip;
        std::string m_current_clip_name;
        std::string m_default_clip_name;
        float m_time = 0.0f; // seconds into the current clip
        bool m_playing = false;

    public:
        explicit SpriteAnimatorComponent(Entity& entity);

        void enter_play() override;
        void tick(float delta_time) override;

        std::string to_string() const override;

        void add_clip(const std::string& name, AnimationClipRef clip);
        const AnimationClips& get_clips() const;
        void set_clips(AnimationClips clips);
        void clear_clips();

        void set_default_clip(const std::string& name);
        const std::string& get_default_clip() const;
        const std::string& get_current_clip() const;

        void play(const std::string& name);
        void resume();
        void pause();
        void stop();

        bool is_playing() const;

    private:
        void apply_key_values();
    };
} // namespace hob
