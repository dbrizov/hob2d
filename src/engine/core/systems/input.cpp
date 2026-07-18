#include "input.h"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include "engine/core/assert.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "renderer/renderer.h"
#include "sdl_context.h"

namespace hob {
    InputEvent::InputEvent(const char* ev_name, InputEventType ev_type, float ev_axis_value)
        : name(ev_name)
        , type(ev_type)
        , axis_value(ev_axis_value) {}

    Input::Input(const SdlContext& sdl_context, const Renderer& renderer)
        : m_sdl_context(sdl_context)
        , m_renderer(renderer) {
        m_input_config = InputConfig(PathUtils::get_input_config_path());
        m_digital_sources = m_input_config.digital_sources();

        for (const auto& [axis, _] : m_input_config.axes) {
            m_axis_values[axis] = 0.0f;
        }

        for (const InputSource& source : m_digital_sources) {
            const uint32_t id = pack_source(source);
            m_down_this_frame[id] = false;
            m_down_last_frame[id] = false;
        }

        adopt_any_gamepad();

        log::input.info(
            "Input::Initialise ({} axes, {} digital sources)", m_input_config.axes.size(), m_digital_sources.size());
    }

    Input::~Input() {
        close_gamepad();
        log::input.info("Input::Shutdown");
    }

    void Input::process_event(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                if (m_gamepad == nullptr) {
                    open_gamepad(event.gdevice.which);
                }
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                if (m_gamepad != nullptr && event.gdevice.which == m_gamepad_id) {
                    close_gamepad();
                    adopt_any_gamepad();
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                // Accumulated here, consumed (and reset) in tick.
                m_wheel_delta += event.wheel.y;
                break;

            default:
                break;
        }
    }

    void Input::tick(float delta_time) {
        update_mouse_state();
        update_down_states();

        dispatch_actions();
        dispatch_axes(delta_time);
    }

    void Input::end_frame() {
        // Wheel input is momentary: it lives for exactly one frame. Reset it every frame (even
        // when tick is skipped, e.g. while the console is open) so it can't leak into a later frame.
        m_wheel_delta = 0.0f;
    }

    void Input::update_mouse_state() {
        float x = 0.0f;
        float y = 0.0f;
        m_mouse_button_mask = SDL_GetMouseState(&x, &y);

        // Map window pixels to logical (FBO) pixels. The FBO is blitted to the window with a
        // uniform STRETCH (no letterboxing today), so the mapping is just an axis-wise scale.
        const Vector2 window_size = m_sdl_context.get_window_size();
        const Vector2 logical_size = m_renderer.get_logical_size();

        if (window_size.x > 0.0f && window_size.y > 0.0f) {
            x = x * (logical_size.x / window_size.x);
            y = y * (logical_size.y / window_size.y);
        }

        m_mouse_screen_position = Vector2(x, y);

        // Relative motion since last call (consumes the accumulated delta).
        float dx = 0.0f;
        float dy = 0.0f;
        SDL_GetRelativeMouseState(&dx, &dy);
        m_mouse_delta = Vector2(dx, dy);
    }

    void Input::update_down_states() {
        for (const InputSource& source : m_digital_sources) {
            const uint32_t id = pack_source(source);
            m_down_last_frame[id] = m_down_this_frame[id];
            m_down_this_frame[id] = is_source_down(source);
        }
    }

    void Input::dispatch_event(const InputEvent& event) const {
        for (const auto& entry : m_handlers) {
            entry.handler(event);
        }
    }

    void Input::dispatch_actions() {
        for (const auto& [action, mapping] : m_input_config.actions) {
            bool down_now = false;
            bool down_before = false;
            for (const InputSource& source : mapping.sources) {
                const uint32_t id = pack_source(source);
                down_now = down_now || m_down_this_frame[id];
                down_before = down_before || m_down_last_frame[id];
            }

            if (down_now && !down_before) {
                dispatch_event(InputEvent(action.c_str(), InputEventType::Pressed, 0.0f));
            }
            else if (!down_now && down_before) {
                dispatch_event(InputEvent(action.c_str(), InputEventType::Released, 0.0f));
            }
        }
    }

    void Input::dispatch_axes(float delta_time) {
        auto any_down = [&](const std::vector<InputSource>& sources) {
            for (const InputSource& source : sources) {
                if (m_down_this_frame[pack_source(source)]) {
                    return true;
                }
            }
            return false;
        };

        for (auto& [axis, mapping] : m_input_config.axes) {
            // Keyboard-style ramped value (accel/decel) from the digital sources.
            const bool any_positive = any_down(mapping.positive_sources);
            const bool any_negative = any_down(mapping.negative_sources);

            float& ramped = m_axis_values[axis];

            if ((any_positive && any_negative) || (!any_positive && !any_negative)) {
                if (ramped < 0.0f) {
                    ramped = std::min(ramped + mapping.deceleration * delta_time, 0.0f);
                }
                else if (ramped > 0.0f) {
                    ramped = std::max(ramped - mapping.deceleration * delta_time, 0.0f);
                }
            }
            else if (any_positive) {
                ramped = std::min(ramped + mapping.acceleration * delta_time, 1.0f);
            }
            else if (any_negative) {
                ramped = std::max(ramped - mapping.acceleration * delta_time, -1.0f);
            }

            // Analog sources feed their raw (deadzoned, scaled) value directly; the strongest
            // analog source wins. It is summed with the digital ramp so opposing digital and
            // analog inputs cancel.
            float analog = 0.0f;
            for (const InputSource& source : mapping.analog) {
                const float value = read_analog_source(source) * source.scale;
                if (std::fabs(value) > std::fabs(analog)) {
                    analog = value;
                }
            }

            // Only clamp axes that mix a digital ramp with analog (e.g. movement), where the sum
            // can exceed the [-1, 1] range. Pure-analog axes (e.g. mouse-look / aim sensitivity)
            // keep their raw scaled magnitude.
            const bool has_digital = !mapping.positive_sources.empty() || !mapping.negative_sources.empty();
            float result = ramped + analog;
            if (has_digital) {
                result = std::clamp(result, -1.0f, 1.0f);
            }

            dispatch_event(InputEvent(axis.c_str(), InputEventType::Axis, result));
        }
    }

    bool Input::is_source_down(const InputSource& source) const {
        // A trigger can drive a digital binding: it counts as "down"
        // once its deadzoned magnitude crosses a press threshold.
        if (source.is_analog) {
            return std::fabs(read_analog_source(source)) >= m_input_config.gamepad.trigger_button_threshold;
        }

        switch (source.device) {
            case InputDevice::Keyboard: {
                const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
                return keyboard_state[source.code];
            }

            case InputDevice::Mouse:
                switch (static_cast<MouseCode>(source.code)) {
                    case MouseCode::ButtonLeft:
                        return (m_mouse_button_mask & SDL_BUTTON_LMASK) != 0;
                    case MouseCode::ButtonRight:
                        return (m_mouse_button_mask & SDL_BUTTON_RMASK) != 0;
                    case MouseCode::ButtonMiddle:
                        return (m_mouse_button_mask & SDL_BUTTON_MMASK) != 0;
                    case MouseCode::WheelUp:
                        return m_wheel_delta > 0.0f;
                    case MouseCode::WheelDown:
                        return m_wheel_delta < 0.0f;
                    default:
                        return false;
                }

            case InputDevice::Gamepad:
                if (m_gamepad == nullptr) {
                    return false;
                }
                return SDL_GetGamepadButton(m_gamepad, static_cast<SDL_GamepadButton>(source.code));
        }

        return false;
    }

    float Input::read_analog_source(const InputSource& source) const {
        switch (source.device) {
            case InputDevice::Mouse:
                switch (static_cast<MouseCode>(source.code)) {
                    case MouseCode::AxisX:
                        return m_mouse_delta.x;
                    case MouseCode::AxisY:
                        return m_mouse_delta.y;
                    case MouseCode::AxisWheel:
                        return m_wheel_delta;
                    default:
                        return 0.0f;
                }

            case InputDevice::Gamepad: {
                if (m_gamepad == nullptr) {
                    return 0.0f;
                }

                const auto axis = static_cast<SDL_GamepadAxis>(source.code);
                const float raw = static_cast<float>(SDL_GetGamepadAxis(m_gamepad, axis));

                const bool is_trigger =
                    (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
                if (is_trigger) {
                    // Triggers report 0..32767.
                    const float value = std::clamp(raw / 32767.0f, 0.0f, 1.0f);
                    const float deadzone = m_input_config.gamepad.trigger_deadzone;
                    if (value < deadzone) {
                        return 0.0f;
                    }
                    return (value - deadzone) / (1.0f - deadzone);
                }

                // Sticks report -32768..32767.
                const float value = std::clamp(raw / 32767.0f, -1.0f, 1.0f);
                const float deadzone = m_input_config.gamepad.stick_deadzone;
                const float magnitude = std::fabs(value);
                if (magnitude < deadzone) {
                    return 0.0f;
                }

                // Rescale so output ramps from 0 at the deadzone edge to 1 at full deflection.
                const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
                return std::copysign(scaled, value);
            }

            default:
                return 0.0f;
        }
    }

    void Input::open_gamepad(uint32_t gamepad_id) {
        m_gamepad = SDL_OpenGamepad(gamepad_id);
        if (m_gamepad == nullptr) {
            log::input.error("SDL_OpenGamepad failed: {}", SDL_GetError());
            return;
        }

        m_gamepad_id = gamepad_id;
        log::input.info("Gamepad connected: {}", SDL_GetGamepadName(m_gamepad));
    }

    void Input::close_gamepad() {
        if (m_gamepad == nullptr) {
            return;
        }

        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
        m_gamepad_id = 0;
        log::input.info("Gamepad disconnected");
    }

    void Input::adopt_any_gamepad() {
        if (m_gamepad != nullptr) {
            return;
        }

        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids == nullptr) {
            return;
        }

        if (count > 0) {
            open_gamepad(ids[0]);
        }

        SDL_free(ids);
    }

    InputEventHandlerId Input::add_input_event_handler(InputEventHandler handler) {
        const InputEventHandlerId handler_id = m_next_handler_id;
        m_next_handler_id += 1;

        m_handler_index_by_id[handler_id] = static_cast<InputEventHandlerIndex>(m_handlers.size());
        m_handlers.emplace_back(handler_id, std::move(handler));

        return handler_id;
    }

    bool Input::remove_input_event_handler(InputEventHandlerId id) {
        auto it = m_handler_index_by_id.find(id);
        if (it == m_handler_index_by_id.end()) {
            return false;
        }

        // Swap-pop; fix the moved handler's stored index.
        const InputEventHandlerIndex index = it->second;
        HOB_ASSERT(index < m_handlers.size(), "InputEventHandler index/map desynced");
        const InputEventHandlerIndex last_index = static_cast<InputEventHandlerIndex>(m_handlers.size() - 1);
        if (index != last_index) {
            m_handlers[index] = std::move(m_handlers[last_index]);
            m_handler_index_by_id[m_handlers[index].handler_id] = index;
        }

        m_handlers.pop_back();
        m_handler_index_by_id.erase(it);
        return true;
    }

    Vector2 Input::get_mouse_screen_position() const {
        return m_mouse_screen_position;
    }

    bool Input::is_gamepad_connected() const {
        return m_gamepad != nullptr;
    }

    uint32_t Input::pack_source(const InputSource& source) {
        return (static_cast<uint32_t>(source.device) << 16) | (static_cast<uint32_t>(source.code) & 0xFFFF);
    }
} // namespace hob
