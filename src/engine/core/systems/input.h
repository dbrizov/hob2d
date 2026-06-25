#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_events.h>

#include "engine/math/vector2.h"
#include "input_config.h"

struct SDL_Gamepad;

namespace hob {
    class SdlContext;
    class Renderer;

    enum class InputEventType {
        Axis,
        Pressed,
        Released,
    };

    struct InputEvent {
        const char* name;
        InputEventType type;
        float axis_value;

        InputEvent(const char* ev_name, InputEventType ev_type, float ev_axis_value);
    };

    using InputEventHandler = std::function<void(const InputEvent&)>;
    using InputEventHandlerId = int32_t;
    using InputEventHandlerIndex = uint32_t;

    constexpr InputEventHandlerId INVALID_INPUT_EVENT_HANDLER_ID = -1;

    class Input {
        const SdlContext& m_sdl_context;
        const Renderer& m_renderer;

        struct HandlerEntry {
            InputEventHandlerId handler_id;
            InputEventHandler handler;
        };

        InputEventHandlerId m_next_handler_id = 0;
        std::vector<HandlerEntry> m_handlers;
        std::unordered_map<InputEventHandlerId, InputEventHandlerIndex> m_handler_index_by_id;

        InputConfig m_input_config;
        std::unordered_map<std::string, float> m_axis_values;

        // Digital source edge tracking. Keyed by a packed (device, code) id.
        std::vector<InputSource> m_digital_sources;
        std::unordered_map<uint32_t, bool> m_down_this_frame;
        std::unordered_map<uint32_t, bool> m_down_last_frame;

        // Per-frame device state, refreshed in tick / process_event.
        Vector2 m_mouse_screen_position;
        Vector2 m_mouse_delta;
        uint32_t m_mouse_button_mask = 0;
        float m_wheel_delta = 0.0f;

        // Single gamepad (player 1) with hotplug. Null when none connected.
        SDL_Gamepad* m_gamepad = nullptr;
        uint32_t m_gamepad_id = 0;

    public:
        Input(const SdlContext& sdl_context, const Renderer& renderer);
        ~Input();

        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;

        Input(Input&&) = delete;
        Input& operator=(Input&&) = delete;

        void process_event(const SDL_Event& event);
        void tick(float delta_time);
        void end_frame();

        InputEventHandlerId add_input_event_handler(InputEventHandler handler);
        bool remove_input_event_handler(InputEventHandlerId id);

        Vector2 get_mouse_screen_position() const;
        bool is_gamepad_connected() const;

    private:
        void update_mouse_state();
        void update_down_states();

        void dispatch_event(const InputEvent& event) const;
        void dispatch_actions();
        void dispatch_axes(float delta_time);

        bool is_source_down(const InputSource& source) const;
        float read_analog_source(const InputSource& source) const;

        void open_gamepad(uint32_t gamepad_id);
        void close_gamepad();
        void adopt_any_gamepad();

        static uint32_t pack_source(const InputSource& source);
    };
} // namespace hob
