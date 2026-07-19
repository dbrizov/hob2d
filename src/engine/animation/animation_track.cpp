#include "animation_track.h"

#include "engine/components/sockets_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/entity/entity.h"

namespace hob {
    namespace {
        template<typename T>
        const T* get_key_value(const std::vector<Keyframe<T>>& keys, float time) {
            if (keys.empty()) {
                return nullptr;
            }

            const T* value = &keys.front().value;
            for (const Keyframe<T>& key : keys) {
                if (key.time > time) {
                    break;
                }
                value = &key.value;
            }

            return value;
        }

        template<typename T>
        float get_track_duration(const std::vector<Keyframe<T>>& keys) {
            return keys.empty() ? 0.0f : keys.back().time;
        }

        TransformComponent* find_socket(Entity& entity, const std::string& name) {
            const SocketsComponent* sockets = entity.get_component<SocketsComponent>();
            return sockets != nullptr ? sockets->get_socket(name) : nullptr;
        }
    } // namespace

    void TextureTrack::apply_key_values(Entity& entity, float time) const {
        SpriteComponent* sprite = entity.get_component<SpriteComponent>();
        if (sprite == nullptr) {
            return;
        }

        const TextureRef* texture = get_key_value(keys, time);
        if (texture != nullptr && *texture != sprite->get_texture()) {
            sprite->set_texture(*texture);
        }
    }

    float TextureTrack::get_duration() const {
        return length;
    }

    void SocketPositionTrack::apply_key_values(Entity& entity, float time) const {
        if (TransformComponent* transform = find_socket(entity, socket)) {
            if (const Vector2* value = get_key_value(keys, time)) {
                transform->set_local_position(*value);
            }
        }
    }

    float SocketPositionTrack::get_duration() const {
        return get_track_duration(keys);
    }

    void SocketRotationTrack::apply_key_values(Entity& entity, float time) const {
        if (TransformComponent* transform = find_socket(entity, socket)) {
            if (const float* value = get_key_value(keys, time)) {
                transform->set_local_rotation(*value);
            }
        }
    }

    float SocketRotationTrack::get_duration() const {
        return get_track_duration(keys);
    }
} // namespace hob
