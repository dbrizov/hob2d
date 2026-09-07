#pragma once

#include <string>

#include <sol/sol.hpp>

#include "editor/editor_field_target.h"
#include "editor_command.h"

namespace hob::editor {
    class EditorCommandRevertOverride : public EditorCommand {
        EditorFieldTarget m_target;
        sol::object m_override;

    public:
        EditorCommandRevertOverride(std::string label, EditorFieldTarget target, sol::object override);

        void undo(Editor& editor) override;
        void redo(Editor& editor) override;
    };
} // namespace hob::editor
