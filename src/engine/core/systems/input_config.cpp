#include "input_config.h"

#include <algorithm>
#include <fstream>
#include <optional>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <nlohmann/json.hpp>

#include "engine/core/debug.h"

namespace hob {
    static std::optional<InputSource> parse_keyboard(const std::string& name) {
        const SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());
        if (scancode == SDL_SCANCODE_UNKNOWN) {
            return std::nullopt;
        }

        return InputSource{InputDevice::Keyboard, static_cast<int>(scancode), false};
    }

    static std::optional<InputSource> parse_mouse(const std::string& token) {
        // token is the part after the "Mouse " prefix.
        struct Entry {
            const char* name;
            MouseCode code;
            bool is_analog;
        };

        static const Entry table[] = {
            {"Left", MouseCode::ButtonLeft, false},
            {"Right", MouseCode::ButtonRight, false},
            {"Middle", MouseCode::ButtonMiddle, false},
            {"WheelUp", MouseCode::WheelUp, false},
            {"WheelDown", MouseCode::WheelDown, false},
            {"X", MouseCode::AxisX, true},
            {"Y", MouseCode::AxisY, true},
            {"Wheel", MouseCode::AxisWheel, true},
        };

        for (const Entry& entry : table) {
            if (token == entry.name) {
                return InputSource{InputDevice::Mouse, static_cast<int>(entry.code), entry.is_analog};
            }
        }

        return std::nullopt;
    }

    static std::optional<InputSource> parse_gamepad(const std::string& token) {
        // token is the part after the "Gamepad " prefix. We use friendly PascalCase names
        // for config consistency (e.g. "Start", "DpadUp", "LeftX") and translate them to
        // SDL's own canonical tokens, which are then resolved via SDL's string lookups.
        static const std::unordered_map<std::string, const char*> button_names = {
            {"A", "a"},
            {"B", "b"},
            {"X", "x"},
            {"Y", "y"},
            {"Back", "back"},
            {"Guide", "guide"},
            {"Start", "start"},
            {"LeftStick", "leftstick"},
            {"RightStick", "rightstick"},
            {"LeftShoulder", "leftshoulder"},
            {"RightShoulder", "rightshoulder"},
            {"DpadUp", "dpup"},
            {"DpadDown", "dpdown"},
            {"DpadLeft", "dpleft"},
            {"DpadRight", "dpright"},
        };

        static const std::unordered_map<std::string, const char*> axis_names = {
            {"LeftX", "leftx"},
            {"LeftY", "lefty"},
            {"RightX", "rightx"},
            {"RightY", "righty"},
            {"LeftTrigger", "lefttrigger"},
            {"RightTrigger", "righttrigger"},
        };

        if (const auto it = button_names.find(token); it != button_names.end()) {
            const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(it->second);
            if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                return InputSource{InputDevice::Gamepad, static_cast<int>(button), false};
            }
        }

        if (const auto it = axis_names.find(token); it != axis_names.end()) {
            const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(it->second);
            if (axis != SDL_GAMEPAD_AXIS_INVALID) {
                return InputSource{InputDevice::Gamepad, static_cast<int>(axis), true};
            }
        }

        return std::nullopt;
    }

    static std::optional<InputSource> parse_source(const std::string& name) {
        constexpr const char* MOUSE_PREFIX = "Mouse ";
        constexpr const char* GAMEPAD_PREFIX = "Gamepad ";

        std::optional<InputSource> source;
        if (name.starts_with(MOUSE_PREFIX)) {
            source = parse_mouse(name.substr(std::char_traits<char>::length(MOUSE_PREFIX)));
        }
        else if (name.starts_with(GAMEPAD_PREFIX)) {
            source = parse_gamepad(name.substr(std::char_traits<char>::length(GAMEPAD_PREFIX)));
        }
        else {
            source = parse_keyboard(name);
        }

        if (!source.has_value()) {
            debug::log_error("Unknown input source: {}", name);
        }

        return source;
    }

    static void parse_source_list(const nlohmann::json& list, std::vector<InputSource>& out) {
        for (const auto& item : list) {
            const std::optional<InputSource> source = parse_source(item.get<std::string>());
            if (!source.has_value()) {
                continue;
            }

            out.push_back(*source);
        }
    }

    InputConfig::InputConfig(const std::filesystem::path& json_path) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            debug::log_error("Cannot open input config file: {}", json_path.string());
            return;
        }

        nlohmann::json json = nlohmann::json::parse(file);

        // Gamepad tuning (optional block).
        if (json.contains("gamepad")) {
            const auto& cfg = json["gamepad"];
            gamepad.stick_deadzone = cfg.value("stick_deadzone", gamepad.stick_deadzone);
            gamepad.trigger_deadzone = cfg.value("trigger_deadzone", gamepad.trigger_deadzone);
            gamepad.trigger_button_threshold = cfg.value("trigger_button_threshold", gamepad.trigger_button_threshold);

            // Deadzones feed a 1/(1 - deadzone) rescale, so keep them strictly below 1 to avoid
            // a divide-by-zero; clamp the button threshold to the normalized [0, 1] range.
            gamepad.stick_deadzone = std::clamp(gamepad.stick_deadzone, 0.0f, 0.99f);
            gamepad.trigger_deadzone = std::clamp(gamepad.trigger_deadzone, 0.0f, 0.99f);
            gamepad.trigger_button_threshold = std::clamp(gamepad.trigger_button_threshold, 0.0f, 1.0f);
        }

        // Actions
        for (auto& [action_name, sources] : json["action_mappings"].items()) {
            ActionConfig action_config;
            parse_source_list(sources, action_config.sources);
            actions[action_name] = std::move(action_config);
        }

        // Axes
        for (auto& [axis_name, cfg] : json["axis_mappings"].items()) {
            AxisConfig axis_config;
            axis_config.acceleration = cfg.value("acceleration", 0.0f);
            axis_config.deceleration = cfg.value("deceleration", 0.0f);

            if (cfg.contains("positive")) {
                parse_source_list(cfg["positive"], axis_config.positive_sources);
            }

            if (cfg.contains("negative")) {
                parse_source_list(cfg["negative"], axis_config.negative_sources);
            }

            if (cfg.contains("analog")) {
                for (const auto& entry : cfg["analog"]) {
                    if (!entry.is_object() || !entry.contains("source")) {
                        debug::log_error("Analog entry in axis '{}' must be an object with a 'source'; ignored",
                                         axis_name);
                        continue;
                    }

                    const std::string source_name = entry.at("source").get<std::string>();
                    const std::optional<InputSource> source = parse_source(source_name);
                    if (!source.has_value()) {
                        continue;
                    }

                    if (!source->is_analog) {
                        debug::log_error("Digital source '{}' used as an analog binding; ignored", source_name);
                        continue;
                    }

                    InputSource analog_source = *source;
                    analog_source.scale = entry.value("scale", 1.0f);
                    axis_config.analog.push_back(analog_source);
                }
            }

            axes[axis_name] = std::move(axis_config);
        }
    }

    std::vector<InputSource> InputConfig::digital_sources() const {
        std::vector<InputSource> sources;
        sources.reserve(32);

        auto add_source = [&sources](const InputSource& source) {
            const auto same = [&source](const InputSource& s) {
                return s.device == source.device && s.code == source.code;
            };

            if (std::find_if(sources.begin(), sources.end(), same) == sources.end()) {
                sources.push_back(source);
            }
        };

        for (const auto& pair : actions) {
            for (const auto& source : pair.second.sources) {
                add_source(source);
            }
        }

        for (const auto& pair : axes) {
            for (const auto& source : pair.second.positive_sources) {
                add_source(source);
            }

            for (const auto& source : pair.second.negative_sources) {
                add_source(source);
            }
        }

        return sources;
    }
} // namespace hob
