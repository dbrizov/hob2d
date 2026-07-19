#include "input_component.h"

#include "engine/core/engine.h"
#include "engine/entity/entity.h"

namespace hob {
    InputComponent::InputComponent(Entity& entity)
        : Component(entity) {}

    int InputComponent::get_priority() const {
        return component_priority::CP_INPUT;
    }

    void InputComponent::enter_play() {
        m_input_event_handler_id = get_engine().get_input().add_input_event_handler([this](const InputEvent& event) {
            this->on_input_event(event);
        });
    }

    void InputComponent::exit_play() {
        get_engine().get_input().remove_input_event_handler(m_input_event_handler_id);
    }

    std::string InputComponent::to_string() const {
        return "InputComponent";
    }

    BindingId InputComponent::bind_axis(const char* axis_name, AxisBindingFunc function) {
        const BindingId binding_id = m_next_binding_id;
        m_next_binding_id += 1;

        auto& axis_bindings = m_axis_bindings[axis_name];
        axis_bindings.emplace_back(binding_id, std::move(function));

        return binding_id;
    }

    void InputComponent::unbind_axis(const char* axis_name, BindingId axis_binding_id) {
        auto it = m_axis_bindings.find(axis_name);
        if (it == m_axis_bindings.end()) {
            return;
        }

        auto& bindings = it->second;
        for (auto iter = bindings.begin(); iter != bindings.end(); ++iter) {
            if (iter->id == axis_binding_id) {
                bindings.erase(iter);
                break;
            }
        }

        if (bindings.empty()) {
            m_axis_bindings.erase(it);
        }
    }

    BindingId InputComponent::bind_action(const char* action_name,
                                          InputEventType event_type,
                                          ActionBindingFunc function) {
        const BindingId binding_id = m_next_binding_id;
        m_next_binding_id += 1;

        if (event_type == InputEventType::Pressed) {
            auto& bindings = m_action_pressed_bindings[action_name];
            bindings.emplace_back(binding_id, std::move(function));
        }
        else if (event_type == InputEventType::Released) {
            auto& bindings = m_action_released_bindings[action_name];
            bindings.emplace_back(binding_id, std::move(function));
        }

        return binding_id;
    }

    void InputComponent::unbind_action(const char* action_name, BindingId action_binding_id) {
        auto erase_from = [&](auto& map) {
            auto it = map.find(action_name);
            if (it == map.end()) {
                return;
            }

            auto& bindings = it->second;
            for (auto iter = bindings.begin(); iter != bindings.end(); ++iter) {
                if (iter->id == action_binding_id) {
                    bindings.erase(iter);
                    break;
                }
            }

            if (bindings.empty()) {
                map.erase(it);
            }
        };

        erase_from(m_action_pressed_bindings);
        erase_from(m_action_released_bindings);
    }

    void InputComponent::clear_all_bindings() {
        m_axis_bindings.clear();
        m_action_pressed_bindings.clear();
        m_action_released_bindings.clear();
    }

    void InputComponent::on_input_event(const InputEvent& event) {
        switch (event.type) {
            case InputEventType::Axis: {
                auto it = m_axis_bindings.find(event.name);
                if (it == m_axis_bindings.end()) {
                    return;
                }

                auto& bindings = it->second;
                for (auto& entry : bindings) {
                    entry.function(event.axis_value);
                }
                break;
            }

            case InputEventType::Pressed: {
                auto it = m_action_pressed_bindings.find(event.name);
                if (it == m_action_pressed_bindings.end()) {
                    return;
                }

                auto& bindings = it->second;
                for (auto& entry : bindings) {
                    entry.function();
                }
                break;
            }

            case InputEventType::Released: {
                auto it = m_action_released_bindings.find(event.name);
                if (it == m_action_released_bindings.end()) {
                    return;
                }

                auto& bindings = it->second;
                for (auto& entry : bindings) {
                    entry.function();
                }
                break;
            }

            default:
                break;
        }
    }
} // namespace hob
