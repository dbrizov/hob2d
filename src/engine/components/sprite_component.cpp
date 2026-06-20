#include "sprite_component.h"

#include <format>

#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "transform_component.h"

namespace hob {
    SpriteComponent::SpriteComponent(Entity& entity)
        : Component(entity) {}

    void SpriteComponent::enter_play() {
        m_sprite_draw_id = get_engine().get_renderer().register_sprite_draw();
        get_engine().get_entity_spawner().register_sprite(this);
        m_render_dirty = true;
    }

    void SpriteComponent::exit_play() {
        get_engine().get_entity_spawner().unregister_sprite(this);
        get_engine().get_renderer().unregister_sprite_draw(m_sprite_draw_id);
        m_sprite_draw_id = INVALID_SPRITE_DRAW_ID;
    }

    std::string SpriteComponent::to_string() const {
        return std::format("SpriteComponent(entity_id = {})", get_entity().get_id());
    }

    SpriteDrawId SpriteComponent::get_sprite_draw_id() const {
        return m_sprite_draw_id;
    }

    bool SpriteComponent::consume_render_dirty() {
        const bool was_dirty = m_render_dirty;
        m_render_dirty = false;

        return was_dirty;
    }

    const TextureRef& SpriteComponent::get_texture() const {
        return m_texture;
    }

    void SpriteComponent::set_texture(TextureRef texture) {
        m_texture = std::move(texture);
        m_render_dirty = true;
    }

    void SpriteComponent::set_texture(const std::string& path) {
        m_texture = get_engine().get_renderer().get_or_load_texture(path);
        m_render_dirty = true;
    }

    void SpriteComponent::clear_texture() {
        m_texture.reset();
        m_render_dirty = true;
    }

    const Material& SpriteComponent::get_material() const {
        return m_material;
    }

    Material& SpriteComponent::get_material() {
        // Non-const access hands out a mutable reference (used by Lua: sprite:get_material().tint = ...),
        // so conservatively mark dirty — we can't observe the mutation itself. The world draw pass
        // reads material via the const overload, so it does not trip this.
        m_render_dirty = true;
        return m_material;
    }

    void SpriteComponent::set_material(const Material& material) {
        m_material = material;
        m_render_dirty = true;
    }

    Vector2 SpriteComponent::get_pivot() const {
        return m_pivot;
    }

    void SpriteComponent::set_pivot(const Vector2& pivot) {
        m_pivot = pivot;
        m_render_dirty = true;
    }

    Vector2 SpriteComponent::get_scale() const {
        return m_scale;
    }

    void SpriteComponent::set_scale(const Vector2& scale) {
        m_scale = scale;
        m_render_dirty = true;
    }

    int SpriteComponent::get_z_index() const {
        return m_z_index;
    }

    void SpriteComponent::set_z_index(int z_index) {
        m_z_index = z_index;
        m_render_dirty = true;
    }

    int SpriteComponent::get_pixels_per_meter() const {
        return m_pixels_per_meter;
    }

    float SpriteComponent::get_pixels_per_meter_f() const {
        return static_cast<float>(m_pixels_per_meter);
    }

    void SpriteComponent::set_pixels_per_meter(int value) {
        m_pixels_per_meter = value;
        m_render_dirty = true;
    }
} // namespace hob
