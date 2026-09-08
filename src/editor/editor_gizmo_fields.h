#pragma once

#include <memory>
#include <vector>

#include <sol/sol.hpp>

#include "editor/commands/editor_command.h"
#include "editor/commands/editor_command_set_field.h"
#include "editor/editor_field_target.h"
#include "editor/editor_instance_id.h"
#include "engine/entity/entity.h"
#include "engine/math/mathf.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob::editor {
    class Editor;

    inline EditorFieldTarget make_transform_target(EntityId entity_id,
                                                   EditorInstanceId instance_id,
                                                   const char* section,
                                                   const char* field) {
        return EditorFieldTarget{
            .entity_id = entity_id,
            .instance_id = instance_id,
            .is_lua = false,
            .component_key = section,
            .field = field,
        };
    }

    inline bool is_field_changed(float a, float b) {
        return !math::approx_equal(a, b);
    }

    inline bool is_field_changed(const Vector2& a, const Vector2& b) {
        return a != b;
    }

    inline bool is_field_changed(const Vector3& a, const Vector3& b) {
        return a != b;
    }

    template<typename T>
    bool try_set_transform_field(
        Editor& editor, sol::state& lua, const EditorFieldTarget& target, const T& current_value, const T& value) {
        if (!is_field_changed(current_value, value)) {
            return false;
        }

        EditorCommandSetField::apply(editor, target, sol::make_object(lua, value));
        return true;
    }

    template<typename T>
    void push_transform_command_if_changed(std::vector<std::unique_ptr<EditorCommand>>& commands,
                                           sol::state& lua,
                                           const char* label,
                                           const EditorFieldTarget& target,
                                           const T& old_value,
                                           const T& new_value) {
        if (!is_field_changed(old_value, new_value)) {
            return;
        }

        commands.push_back(std::make_unique<EditorCommandSetField>(
            label, target, sol::make_object(lua, old_value), sol::make_object(lua, new_value)));
    }
} // namespace hob::editor
