#pragma once

#include <string>

#include <sol/sol.hpp>

#include "editor/editor_field_target.h"
#include "editor_command.h"

namespace hob::editor {
    class EditorCommandApplyToPrefab : public EditorCommand {
        EditorFieldTarget m_instance_target;
        EditorFieldTarget m_prefab_target;
        sol::object m_value;
        sol::object m_previous_prefab_value; // nil when the prefab did not declare the field

    public:
        EditorCommandApplyToPrefab(std::string label,
                                   EditorFieldTarget instance_target,
                                   EditorFieldTarget prefab_target,
                                   sol::object value,
                                   sol::object previous_prefab_value);

        void undo(Editor& editor) override;
        void redo(Editor& editor) override;
    };
} // namespace hob::editor
