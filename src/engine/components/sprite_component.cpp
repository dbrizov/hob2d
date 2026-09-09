#include "sprite_component.h"

#include <cmath>

#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "transform_component.h"

namespace hob {
    SpriteComponent::SpriteComponent(Entity& entity)
        : Component(entity) {
        m_material = get_engine().get_renderer().get_default_material();
    }

    void SpriteComponent::enter_world() {
        m_sprite_draw_id = get_engine().get_renderer().register_sprite_draw();
        get_engine().get_entity_spawner().register_sprite(this);
        m_render_dirty = true;
    }

    void SpriteComponent::exit_world() {
        get_engine().get_entity_spawner().unregister_sprite(this);
        get_engine().get_renderer().unregister_sprite_draw(m_sprite_draw_id);
        m_sprite_draw_id = INVALID_SPRITE_DRAW_ID;
    }

    std::string SpriteComponent::to_string() const {
        return "SpriteComponent";
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

    void SpriteComponent::set_texture(std::string_view relative_path) {
        m_texture = get_engine().get_renderer().get_or_load_texture(relative_path);
        m_render_dirty = true;
    }

    void SpriteComponent::clear_texture() {
        m_texture.reset();
        m_render_dirty = true;
    }

    const MaterialRef& SpriteComponent::get_material() const {
        return m_material;
    }

    MaterialRef SpriteComponent::get_material() {
        // Non-const access may mutate the material; the draw pass reads via the const overload.
        m_render_dirty = true;
        return m_material;
    }

    void SpriteComponent::set_material(MaterialRef material) {
        m_material = std::move(material);
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

    int32_t SpriteComponent::get_z_index() const {
        return m_z_index;
    }

    void SpriteComponent::set_z_index(int32_t z_index) {
        m_z_index = z_index;
        m_render_dirty = true;
    }

    uint32_t SpriteComponent::get_pixels_per_meter() const {
        return m_pixels_per_meter;
    }

    float SpriteComponent::get_pixels_per_meter_f() const {
        return static_cast<float>(m_pixels_per_meter);
    }

    void SpriteComponent::set_pixels_per_meter(uint32_t value) {
        m_pixels_per_meter = value;
        m_render_dirty = true;
    }

    Vector2 SpriteComponent::get_local_size() const {
        if (!m_texture) {
            return Vector2::zero();
        }

        const float ppm = get_pixels_per_meter_f();

        return Vector2((static_cast<float>(m_texture->get_width()) / ppm) * m_scale.x,
                       (static_cast<float>(m_texture->get_height()) / ppm) * m_scale.y);
    }

    AABB SpriteComponent::get_local_rect() const {
        const Vector2 size = get_local_size();
        const Vector2 center((0.5f - m_pivot.x) * size.x, (m_pivot.y - 0.5f) * size.y);
        const Vector2 extents(std::abs(size.x) * 0.5f, std::abs(size.y) * 0.5f);

        return AABB(center, extents);
    }
} // namespace hob
