#include "animation_track.h"

#include "engine/components/sprite_component.h"
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
} // namespace hob
