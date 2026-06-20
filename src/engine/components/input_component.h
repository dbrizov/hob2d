#pragma once

#include "component.h"
#include "engine/core/systems/input.h"

namespace hob {
    using BindingId = int32_t;
    using AxisBindingFunc = std::function<void(float)>;
    using ActionBindingFunc = std::function<void()>;

    constexpr BindingId INVALID_BINDING_ID = -1;

    class InputComponent : public Component {
        struct AxisBindingEntry {
            BindingId id;
            AxisBindingFunc function;
        };

        struct ActionBindingEntry {
            BindingId id;
            ActionBindingFunc function;
        };

        InputEventHandlerId m_input_event_handler_id = INVALID_INPUT_EVENT_HANDLER_ID;

        BindingId m_next_binding_id = 0;
        std::unordered_map<std::string, std::vector<AxisBindingEntry>> m_axis_bindings;
        std::unordered_map<std::string, std::vector<ActionBindingEntry>> m_action_pressed_bindings;
        std::unordered_map<std::string, std::vector<ActionBindingEntry>> m_action_released_bindings;

    public:
        explicit InputComponent(Entity& entity);

        int get_priority() const override;

        void enter_play() override;
        void exit_play() override;

        std::string to_string() const override;

        BindingId bind_axis(const char* axis_name, AxisBindingFunc function);
        void unbind_axis(const char* axis_name, BindingId axis_binding_id);

        BindingId bind_action(const char* action_name, InputEventType event_type, ActionBindingFunc function);
        void unbind_action(const char* action_name, BindingId action_binding_id);

        void clear_all_bindings();

    private:
        void on_input_event(const InputEvent& event);
    };
} // namespace hob
