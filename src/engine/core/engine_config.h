#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "engine/core/aspect_mode.h"
#include "engine/core/systems/renderer/sampler.h"
#include "engine/core/systems/window.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob {
    struct GraphicsConfig {
        std::string window_title = "Hob2D";
        std::string window_icon = "icons/hob_icon.ico";
        uint32_t window_width = 1152;
        uint32_t window_height = 648;
        uint32_t reference_width = 1920;
        uint32_t reference_height = 1080;
        AspectMode aspect_mode = AspectMode::expand;
        float render_scale = 1.0f;
        uint32_t target_fps = 60;
        bool vsync_enabled = true;
        TextureFilter default_texture_filter = TextureFilter::Linear;
        TextureWrap default_texture_wrap = TextureWrap::Clamp;
    };

    struct UiSystemConfig {
        uint32_t reference_width = 1920;
        uint32_t reference_height = 1080;
        AspectMode aspect_mode = AspectMode::expand;
    };

    struct PhysicsConfig {
        Vector2 gravity = Vector2(0.0f, -9.81f);
        uint32_t ticks_per_second = 60;
        uint32_t sub_steps_per_tick = 4;
        bool interpolation_enabled = true;
    };

    struct PhysicsConfig3D {
        Vector3 gravity = Vector3(0.0f, -9.81f, 0.0f);
        uint32_t ticks_per_second = 60;
        uint32_t sub_steps_per_tick = 4;
        bool interpolation_enabled = true;
    };

    struct AudioConfig {
        float master_volume = 1.0f;
        bool enabled = true;
    };

    // Boot policy a host driving the engine (the editor) overrides. Never read from JSON.
    struct HostConfig {
        std::optional<WindowConfig> main_window_override;
        bool main_window_hosts_game = true;
        bool run_project_main_on_boot = true;
    };

    struct EngineConfig {
        GraphicsConfig graphics_config;
        UiSystemConfig ui_system_config;
        PhysicsConfig physics_config;
        PhysicsConfig3D physics_config_3d;
        AudioConfig audio_config;
        HostConfig host_config;

        EngineConfig() = default;
        explicit EngineConfig(const std::filesystem::path& json_path);
    };
} // namespace hob
