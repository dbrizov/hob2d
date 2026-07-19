#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/core/systems/renderer/texture.h"
#include "engine/math/vector2.h"

namespace hob {
    class Entity;

    template<typename T>
    struct Keyframe {
        float time = 0.0f; // seconds
        T value{};
    };

    class AnimationTrack {
    public:
        virtual ~AnimationTrack() = default;

        virtual void apply_key_values(Entity& entity, float time) const = 0;
        virtual float get_duration() const = 0;
    };

    using AnimationTrackRef = std::unique_ptr<AnimationTrack>;

    class TextureTrack : public AnimationTrack {
    public:
        std::vector<Keyframe<TextureRef>> keys;
        float length = 0.0f;

        void apply_key_values(Entity& entity, float time) const override;
        float get_duration() const override;
    };

    class SocketPositionTrack : public AnimationTrack {
    public:
        std::vector<Keyframe<Vector2>> keys;
        std::string socket;

        void apply_key_values(Entity& entity, float time) const override;
        float get_duration() const override;
    };

    class SocketRotationTrack : public AnimationTrack {
    public:
        std::vector<Keyframe<float>> keys;
        std::string socket;

        void apply_key_values(Entity& entity, float time) const override;
        float get_duration() const override;
    };
} // namespace hob
