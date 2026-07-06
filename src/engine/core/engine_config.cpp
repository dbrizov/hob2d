#include "engine_config.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "debug.h"
#include "logging.h"

namespace hob {
    EngineConfig::EngineConfig(const std::filesystem::path& json_path) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            log::engine.error("Cannot open engine config file: {}", json_path.string());
            return;
        }

        nlohmann::json json = nlohmann::json::parse(file);

        if (json.contains("graphics")) {
            const auto& g = json["graphics"];
            graphics_config.window_title = g.value("window_title", graphics_config.window_title);
            graphics_config.window_width = g.value("window_width", graphics_config.window_width);
            graphics_config.window_height = g.value("window_height", graphics_config.window_height);
            graphics_config.reference_width = g.value("reference_width", graphics_config.reference_width);
            graphics_config.reference_height = g.value("reference_height", graphics_config.reference_height);

            if (g.contains("aspect_mode")) {
                const auto str = g["aspect_mode"].get<std::string>();
                if (!aspect_mode_from_string(str, graphics_config.aspect_mode)) {
                    log::engine.error(
                        "Unknown aspect_mode '{}' (keep_width|keep_height|expand|shrink), defaulting to 'expand'", str);
                    graphics_config.aspect_mode = AspectMode::expand;
                }
            }

            graphics_config.render_scale = g.value("render_scale", graphics_config.render_scale);
            graphics_config.target_fps = g.value("target_fps", graphics_config.target_fps);
            graphics_config.vsync_enabled = g.value("vsync_enabled", graphics_config.vsync_enabled);

            if (g.contains("default_texture_filter")) {
                const auto str = g["default_texture_filter"].get<std::string>();
                if (!texture_filter_from_string(str, graphics_config.default_texture_filter)) {
                    log::engine.error("engine_config: unknown default_texture_filter '{}' (expected nearest|linear)",
                                      str);
                }
            }

            if (g.contains("default_texture_wrap")) {
                const auto str = g["default_texture_wrap"].get<std::string>();
                if (!texture_wrap_from_string(str, graphics_config.default_texture_wrap)) {
                    log::engine.error("engine_config: unknown default_texture_wrap '{}' (expected clamp|repeat|mirror)",
                                      str);
                }
            }
        }

        if (json.contains("ui")) {
            const auto& u = json["ui"];
            ui_system_config.reference_width = u.value("reference_width", ui_system_config.reference_width);
            ui_system_config.reference_height = u.value("reference_height", ui_system_config.reference_height);

            if (u.contains("aspect_mode")) {
                const auto str = u["aspect_mode"].get<std::string>();
                if (!aspect_mode_from_string(str, ui_system_config.aspect_mode)) {
                    log::engine.error(
                        "Unknown aspect_mode '{}' (keep_width|keep_height|expand|shrink), defaulting to 'expand'", str);
                    ui_system_config.aspect_mode = AspectMode::expand;
                }
            }
        }

        if (json.contains("physics")) {
            const auto& p = json["physics"];
            if (p.contains("gravity")) {
                const auto& gravity = p["gravity"];
                physics_config.gravity.x = gravity.value("x", physics_config.gravity.x);
                physics_config.gravity.y = gravity.value("y", physics_config.gravity.y);
            }

            physics_config.ticks_per_second = p.value("ticks_per_second", physics_config.ticks_per_second);
            physics_config.sub_steps_per_tick = p.value("sub_steps_per_tick", physics_config.sub_steps_per_tick);
            physics_config.interpolation_enabled =
                p.value("interpolation_enabled", physics_config.interpolation_enabled);
        }

        if (json.contains("audio")) {
            const auto& a = json["audio"];
            audio_config.master_volume = a.value("master_volume", audio_config.master_volume);
            audio_config.enabled = a.value("enabled", audio_config.enabled);
        }
    }
} // namespace hob
