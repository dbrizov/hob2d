#include "editor_command_add_prefab_section.h"

#include <utility>

#include "editor/editor_prefabs.h"

namespace hob::editor {
    EditorCommandAddPrefabSection::EditorCommandAddPrefabSection(std::string label,
                                                                 std::string prefab_name,
                                                                 std::string key,
                                                                 bool is_lua)
        : EditorCommand(std::move(label))
        , m_prefab_name(std::move(prefab_name))
        , m_key(std::move(key))
        , m_is_lua(is_lua) {}

    void EditorCommandAddPrefabSection::undo(Editor& editor) {
        m_removed = remove_prefab_section(editor, m_prefab_name, m_key, m_is_lua);
    }

    void EditorCommandAddPrefabSection::redo(Editor& editor) {
        add_prefab_section(editor, m_prefab_name, m_key, m_is_lua, m_removed);
    }
} // namespace hob::editor
