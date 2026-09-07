#include "editor_command_add_instance.h"

#include <utility>

namespace hob::editor {
    EditorCommandAddInstance::EditorCommandAddInstance(std::string label, sol::table instance, int32_t index)
        : EditorCommand(std::move(label))
        , m_instance(std::move(instance))
        , m_index(index) {}

    void EditorCommandAddInstance::undo(Editor& editor) {
        m_index = remove_instance(editor, m_instance_id);
    }

    void EditorCommandAddInstance::redo(Editor& editor) {
        m_instance_id = add_instance(editor, m_instance, m_index);
    }
} // namespace hob::editor
