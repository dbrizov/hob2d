#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace hob {
    enum class InputDevice {
        Keyboard,
        Mouse,
        Gamepad,
    };

    // `code` values for InputSource when device == Mouse. SDL has no string lookup
    // for the mouse, so these are resolved by a small table in input_config.cpp and
    // interpreted by Input.
    enum class MouseCode {
        ButtonLeft,
        ButtonRight,
        ButtonMiddle,
        WheelUp,   // digital: fires for one frame on wheel up
        WheelDown, // digital: fires for one frame on wheel down
        AxisX,     // analog: motion delta x
        AxisY,     // analog: motion delta y
        AxisWheel, // analog: wheel delta y
    };

    // A single physical input. `code` is interpreted per device:
    //   Keyboard -> SDL_Scancode
    //   Mouse    -> MouseCode
    //   Gamepad  -> SDL_GamepadButton (digital) or SDL_GamepadAxis (analog)
    // `scale` only applies to analog axis sources: it handles inversion (-1) and sensitivity
    // (e.g. 0.05 for mouse motion). It is read from config, never encoded in the source name,
    // and is ignored for digital sources.
    struct InputSource {
        InputDevice device = InputDevice::Keyboard;
        int code = 0;
        bool is_analog = false;
        float scale = 1.0f;
    };

    struct ActionConfig {
        std::vector<InputSource> sources;
    };

    struct AxisConfig {
        float acceleration = 0.0f;
        float deceleration = 0.0f;
        std::vector<InputSource> positive_sources;
        std::vector<InputSource> negative_sources;
        std::vector<InputSource> analog;
    };

    struct GamepadTuning {
        float stick_deadzone = 0.2f;
        float trigger_deadzone = 0.1f;
        float trigger_button_threshold = 0.5f; // Threshold at which a trigger bound to a digital action counts as a pressed button.
    };

    struct InputConfig {
        std::unordered_map<std::string, ActionConfig> actions;
        std::unordered_map<std::string, AxisConfig> axes;
        GamepadTuning gamepad;

        InputConfig() = default;
        explicit InputConfig(const std::filesystem::path& json_path);

        // Sources used in digital/button positions (actions, axis positive/negative),
        // deduplicated. Used by Input to know which sources to sample for edge
        // (press/release) detection. Excludes the analog axis lists.
        std::vector<InputSource> digital_sources() const;
    };
} // namespace hob
