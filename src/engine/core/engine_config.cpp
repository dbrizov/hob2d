#include "engine_config.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "debug.h"

namespace hob {
    EngineConfig::EngineConfig(const std::filesystem::path& json_path) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            debug::log_error("Cannot open engine config file: {}", json_path.string());
            return;
        }

        nlohmann::json json = nlohmann::json::parse(file);

        if (json.contains("graphics")) {
            const auto& g = json["graphics"];
            graphics_config.window_title = g.value("window_title", graphics_config.window_title);
            graphics_config.window_width = g.value("window_width", graphics_config.window_width);
            graphics_config.window_height = g.value("window_height", graphics_config.window_height);
            graphics_config.logical_width = g.value("logical_width", graphics_config.logical_width);
            graphics_config.logical_height = g.value("logical_height", graphics_config.logical_height);
            graphics_config.target_fps = g.value("target_fps", graphics_config.target_fps);
            graphics_config.vsync_enabled = g.value("vsync_enabled", graphics_config.vsync_enabled);
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

        if (json.contains("ui")) {
            const auto& u = json["ui"];
            ui_system_config.reference_width = u.value("reference_width", ui_system_config.reference_width);
            ui_system_config.reference_height = u.value("reference_height", ui_system_config.reference_height);
            if (u.contains("screen_match_mode")) {
                ui_system_config.screen_match_mode = to_screen_match_mode(u["screen_match_mode"].get<std::string>());
            }
        }
    }
} // namespace hob
