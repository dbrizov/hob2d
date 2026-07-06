#pragma once

#include <filesystem>
#include <string>

#include "engine/core/aspect_mode.h"
#include "engine/core/systems/renderer/sampler.h"
#include "engine/math/vector2.h"

namespace hob {
    struct GraphicsConfig {
        std::string window_title = "Hob2D";
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

    struct AudioConfig {
        float master_volume = 1.0f;
        uint32_t channels = 32;
        bool enabled = true;
    };

    struct EngineConfig {
        GraphicsConfig graphics_config;
        UiSystemConfig ui_system_config;
        PhysicsConfig physics_config;
        AudioConfig audio_config;

        EngineConfig() = default;
        explicit EngineConfig(const std::filesystem::path& json_path);
    };
} // namespace hob
