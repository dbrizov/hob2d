#pragma once

#include <string>

#include <sol/sol.hpp>

#include "editor/editor_instances.h"
#include "editor/editor_selection.h"
#include "editor_command.h"

namespace hob::editor {
    class EditorCommandAddInstance : public EditorCommand {
        // Owning the def is what keeps it alive across an undo, once the scene document has dropped it.
        sol::table m_instance;
        EditorInstanceId m_instance_id = INVALID_EDITOR_INSTANCE_ID;
        int32_t m_index = APPEND_INSTANCE_INDEX;

    public:
        EditorCommandAddInstance(std::string label, sol::table instance, int32_t index = APPEND_INSTANCE_INDEX);

        void undo(Editor& editor) override;
        void redo(Editor& editor) override;
    };
} // namespace hob::editor
