#include "editor_menu_bar.h"

#include <string>
#include <vector>

#include "editor/actions/editor_action.h"
#include "editor/editor.h"
#include "editor/editor_gui_utils.h"

namespace hob::editor {
    void EditorMenuBar::draw(Editor& editor) {
        if (begin_menu("File")) {
            action_menu_item(editor, EditorActionId::NewPrefab);
            action_menu_item(editor, EditorActionId::NewScene);

            const std::vector<std::string> scene_names = editor.get_scene_names();
            if (begin_submenu("Open Scene", !scene_names.empty())) {
                const std::string& current = editor.get_current_scene();
                for (const std::string& name : scene_names) {
                    if (menu_item(name.c_str(), nullptr, true, name == current)) {
                        editor.request_open_scene(name);
                    }
                }
                end_submenu();
            }

            action_menu_item(editor, EditorActionId::Save);
            action_menu_item(editor, EditorActionId::SaveSceneAs);
            action_menu_item(editor, EditorActionId::Quit);
            end_menu();
        }

        if (begin_menu("Edit")) {
            action_menu_item(editor, EditorActionId::Undo);
            action_menu_item(editor, EditorActionId::Redo);
            action_menu_item(editor, EditorActionId::DuplicateSelection);
            action_menu_item(editor, EditorActionId::DeleteSelection);
            end_menu();
        }

        if (begin_menu("Editor")) {
            action_menu_item(editor, EditorActionId::ResetLayout);
            action_menu_item(editor, EditorActionId::RefreshAssets);
            end_menu();
        }
    }
} // namespace hob::editor
