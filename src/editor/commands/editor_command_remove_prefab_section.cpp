#include "editor_command_remove_prefab_section.h"

#include <utility>

#include "editor/editor_prefabs.h"

namespace hob::editor {
    EditorCommandRemovePrefabSection::EditorCommandRemovePrefabSection(std::string label,
                                                                       std::string prefab_name,
                                                                       std::string key,
                                                                       bool is_lua)
        : EditorCommand(std::move(label))
        , m_prefab_name(std::move(prefab_name))
        , m_key(std::move(key))
        , m_is_lua(is_lua) {}

    void EditorCommandRemovePrefabSection::undo(Editor& editor) {
        add_prefab_section(editor, m_prefab_name, m_key, m_is_lua, m_removed);
    }

    void EditorCommandRemovePrefabSection::redo(Editor& editor) {
        m_removed = remove_prefab_section(editor, m_prefab_name, m_key, m_is_lua);
    }
} // namespace hob::editor
