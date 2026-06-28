#pragma once

#include <limits>
#include <memory>
#include <string>

#include "component.h"
#include "engine/core/systems/renderer/material.h"
#include "engine/core/systems/renderer/sprite_draw_data.h"
#include "engine/core/systems/renderer/texture.h"
#include "engine/math/vector2.h"

namespace hob {
    using SpriteIndex = uint32_t;
    constexpr SpriteIndex INVALID_SPRITE_INDEX = std::numeric_limits<SpriteIndex>::max();

    class SpriteComponent : public Component {
        friend class EntitySpawner;

        TextureRef m_texture;
        MaterialRef m_material;
        Vector2 m_pivot = Vector2(0.5f, 0.5f);
        Vector2 m_scale = Vector2(1.0f, 1.0f);
        int m_z_index = 0;
        int m_pixels_per_meter = 64;

        // A handle to the Renderer's sprite draw data.
        SpriteDrawId m_sprite_draw_id = INVALID_SPRITE_DRAW_ID;

        // Slot in EntitySpawner's sprite registry.
        SpriteIndex m_sprite_index = INVALID_SPRITE_INDEX;

        // "A draw-affecting property changed since the renderer last consumed it" — lets the
        // world draw pass skip re-resolving a static sprite's draw data every frame.
        bool m_render_dirty = true;

    public:
        explicit SpriteComponent(Entity& entity);

        void enter_play() override;
        void exit_play() override;

        std::string to_string() const override;

        SpriteDrawId get_sprite_draw_id() const;

        bool consume_render_dirty();

        const TextureRef& get_texture() const;
        void set_texture(TextureRef texture);
        void set_texture(const std::string& relative_path);
        void clear_texture();

        const MaterialRef& get_material() const;
        MaterialRef get_material();
        void set_material(MaterialRef material);

        Vector2 get_pivot() const;
        void set_pivot(const Vector2& pivot);

        Vector2 get_scale() const;
        void set_scale(const Vector2& scale);

        int get_z_index() const;
        void set_z_index(int z_index);

        int get_pixels_per_meter() const;
        float get_pixels_per_meter_f() const;
        void set_pixels_per_meter(int value);
    };
} // namespace hob
