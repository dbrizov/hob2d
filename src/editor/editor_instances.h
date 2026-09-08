#pragma once

#include <string>

#include <sol/sol.hpp>

#include "editor_selection.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob::editor {
    class Editor;

    // Lua indices start at 1, which leaves 0 free to mean "append".
    constexpr int32_t APPEND_INSTANCE_INDEX = 0;

    // Selection follows the document: an instance the editor adds is selected, one it removes is not.
    // Holding that here rather than at the call sites is what makes undo and redo restore it too.
    EditorInstanceId add_instance(Editor& editor, const sol::table& instance, int32_t index);
    int32_t remove_instance(Editor& editor, EditorInstanceId instance_id);

    bool can_edit_scene_instances(const Editor& editor);
    bool can_edit_selected_instances(const Editor& editor);

    void add_prefab_instance(Editor& editor, const std::string& prefab_name, const Vector2& position);
    void add_prefab_instance(Editor& editor, const std::string& prefab_name, const Vector3& position);
    void duplicate_selection(Editor& editor);
    void delete_selection(Editor& editor);
} // namespace hob::editor
