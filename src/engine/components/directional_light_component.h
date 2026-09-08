#pragma once

#include "component.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/color.h"

namespace hob {
    class DirectionalLightComponent : public Component {
        Color m_color = Color(1.0f, 0.96f, 0.9f, 1.0f);
        float m_intensity = 4.0f;
        Color m_sky_color = Color(0.36f, 0.44f, 0.62f, 1.0f);
        Color m_ground_color = Color(0.18f, 0.16f, 0.14f, 1.0f);
        float m_fog_density = 0.02f;
        float m_fog_height = 0.0f;
        float m_fog_falloff = 8.0f;
        float m_fog_anisotropy = 0.5f;
        Color m_fog_color = Color(0.3f, 0.35f, 0.45f, 1.0f);

    public:
        explicit DirectionalLightComponent(Entity& entity);

        void enter_world() override;
        void exit_world() override;

        std::string to_string() const override;

        const Color& get_color() const;
        void set_color(const Color& color);

        float get_intensity() const;
        void set_intensity(float intensity);

        const Color& get_sky_color() const;
        void set_sky_color(const Color& color);

        const Color& get_ground_color() const;
        void set_ground_color(const Color& color);

        float get_fog_density() const;
        void set_fog_density(float density);

        float get_fog_height() const;
        void set_fog_height(float height);

        float get_fog_falloff() const;
        void set_fog_falloff(float falloff);

        float get_fog_anisotropy() const;
        void set_fog_anisotropy(float anisotropy);

        const Color& get_fog_color() const;
        void set_fog_color(const Color& color);

        Vector3 get_direction() const;
        SceneLight build_scene_light() const;
    };
} // namespace hob
