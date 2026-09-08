#include "directional_light_component.h"

#include "engine/core/engine.h"
#include "engine/entity/entity.h"
#include "transform_component_3d.h"

namespace hob {
    namespace {
        Vector3 to_vector3(const Color& color) {
            return Vector3(color.r, color.g, color.b);
        }
    } // namespace

    DirectionalLightComponent::DirectionalLightComponent(Entity& entity)
        : Component(entity) {}

    void DirectionalLightComponent::enter_world() {
        get_engine().set_active_directional_light(this);
    }

    void DirectionalLightComponent::exit_world() {
        get_engine().clear_active_directional_light(this);
    }

    std::string DirectionalLightComponent::to_string() const {
        return "DirectionalLightComponent";
    }

    const Color& DirectionalLightComponent::get_color() const {
        return m_color;
    }

    void DirectionalLightComponent::set_color(const Color& color) {
        m_color = color;
    }

    float DirectionalLightComponent::get_intensity() const {
        return m_intensity;
    }

    void DirectionalLightComponent::set_intensity(float intensity) {
        m_intensity = intensity;
    }

    const Color& DirectionalLightComponent::get_sky_color() const {
        return m_sky_color;
    }

    void DirectionalLightComponent::set_sky_color(const Color& color) {
        m_sky_color = color;
    }

    const Color& DirectionalLightComponent::get_ground_color() const {
        return m_ground_color;
    }

    void DirectionalLightComponent::set_ground_color(const Color& color) {
        m_ground_color = color;
    }

    float DirectionalLightComponent::get_fog_density() const {
        return m_fog_density;
    }

    void DirectionalLightComponent::set_fog_density(float density) {
        m_fog_density = density;
    }

    float DirectionalLightComponent::get_fog_height() const {
        return m_fog_height;
    }

    void DirectionalLightComponent::set_fog_height(float height) {
        m_fog_height = height;
    }

    float DirectionalLightComponent::get_fog_falloff() const {
        return m_fog_falloff;
    }

    void DirectionalLightComponent::set_fog_falloff(float falloff) {
        m_fog_falloff = falloff;
    }

    float DirectionalLightComponent::get_fog_anisotropy() const {
        return m_fog_anisotropy;
    }

    void DirectionalLightComponent::set_fog_anisotropy(float anisotropy) {
        m_fog_anisotropy = anisotropy;
    }

    const Color& DirectionalLightComponent::get_fog_color() const {
        return m_fog_color;
    }

    void DirectionalLightComponent::set_fog_color(const Color& color) {
        m_fog_color = color;
    }

    Vector3 DirectionalLightComponent::get_direction() const {
        return get_entity().get_transform_3d()->get_forward();
    }

    SceneLight DirectionalLightComponent::build_scene_light() const {
        SceneLight light;
        light.direction = get_direction();
        light.color = to_vector3(m_color) * m_intensity;
        light.sky = to_vector3(m_sky_color);
        light.ground = to_vector3(m_ground_color);
        light.ambient = (light.sky + light.ground) * 0.5f;
        light.fog_density = m_fog_density;
        light.fog_height = m_fog_height;
        light.fog_falloff = m_fog_falloff;
        light.fog_anisotropy = m_fog_anisotropy;
        light.fog_color = to_vector3(m_fog_color);
        return light;
    }
} // namespace hob
