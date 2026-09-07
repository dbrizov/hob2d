#include "editor_action.h"

#include <iterator>

#include <imgui_internal.h>

#include "editor/editor.h"
#include "editor/editor_files.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_instances.h"
#include "engine/core/assert.h"

namespace hob::editor {
    namespace {
        constexpr EditorAction ACTIONS[] = {
            {
                .id = EditorActionId::Undo,
                .label = "Undo",
                .chord = ImGuiMod_Ctrl | ImGuiKey_Z,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_commands().can_undo();
                    },
                .format_label =
                    [](const Editor& editor) {
                        return make_command_label("Undo", editor.get_commands().get_undo_label());
                    },
                .run =
                    [](Editor& editor) {
                        editor.get_commands().undo(editor);
                    },
            },
            {
                .id = EditorActionId::Redo,
                .label = "Redo",
                .chord = ImGuiMod_Ctrl | ImGuiKey_Y,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_commands().can_redo();
                    },
                .format_label =
                    [](const Editor& editor) {
                        return make_command_label("Redo", editor.get_commands().get_redo_label());
                    },
                .run =
                    [](Editor& editor) {
                        editor.get_commands().redo(editor);
                    },
            },
            {
                .id = EditorActionId::DuplicateSelection,
                .label = "Duplicate",
                .chord = ImGuiMod_Ctrl | ImGuiKey_D,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return can_edit_selected_instances(editor);
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        duplicate_selection(editor);
                    },
            },
            {
                .id = EditorActionId::DeleteSelection,
                .label = "Delete",
                .chord = ImGuiKey_Delete,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return can_edit_selected_instances(editor);
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        delete_selection(editor);
                    },
            },
            {
                .id = EditorActionId::Play,
                .label = "Play",
                .chord = ImGuiKey_F5,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_state() != WorldState::Playing;
                    },
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_state() == WorldState::Playing;
                    },
                .format_label =
                    [](const Editor& editor) {
                        return std::string(editor.get_state() == WorldState::Paused ? "Resume" : "Play");
                    },
                .run =
                    [](Editor& editor) {
                        editor.set_state(WorldState::Playing);
                    },
            },
            {
                .id = EditorActionId::Pause,
                .label = "Pause",
                .chord = ImGuiKey_F7,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_state() == WorldState::Playing;
                    },
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_state() == WorldState::Paused;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.set_state(WorldState::Paused);
                    },
            },
            {
                .id = EditorActionId::Step,
                .label = "Step",
                .chord = ImGuiKey_F8,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_state() == WorldState::Paused;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.request_step();
                    },
            },
            {
                .id = EditorActionId::Stop,
                .label = "Stop",
                .chord = ImGuiMod_Shift | ImGuiKey_F5,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return editor.get_state() != WorldState::Stopped;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.set_state(WorldState::Stopped);
                    },
            },
            {
                .id = EditorActionId::GizmoTranslateRotate,
                .label = "Translate + Rotate Mode",
                .chord = ImGuiKey_Q,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_scene_view().get_gizmo_mode() == EditorGizmoMode::TranslateRotate;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().set_gizmo_mode(EditorGizmoMode::TranslateRotate);
                    },
            },
            {
                .id = EditorActionId::GizmoTranslate,
                .label = "Translate Mode",
                .chord = ImGuiKey_W,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_scene_view().get_gizmo_mode() == EditorGizmoMode::Translate;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().set_gizmo_mode(EditorGizmoMode::Translate);
                    },
            },
            {
                .id = EditorActionId::GizmoRotate,
                .label = "Rotate Mode",
                .chord = ImGuiKey_E,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_scene_view().get_gizmo_mode() == EditorGizmoMode::Rotate;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().set_gizmo_mode(EditorGizmoMode::Rotate);
                    },
            },
            {
                .id = EditorActionId::GizmoScale,
                .label = "Scale Mode",
                .chord = ImGuiKey_R,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_scene_view().get_gizmo_mode() == EditorGizmoMode::Scale;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().set_gizmo_mode(EditorGizmoMode::Scale);
                    },
            },
            {
                .id = EditorActionId::GizmoToggleSpace,
                .label = "Use Local Space",
                .chord = ImGuiKey_T,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .is_active =
                    [](const Editor& editor) {
                        return editor.get_scene_view().get_gizmo_space() == EditorGizmoSpace::Local;
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().toggle_gizmo_space();
                    },
            },
            {
                .id = EditorActionId::FocusSelection,
                .label = "Focus Selection",
                .chord = ImGuiKey_F,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return !editor.get_selection().ids.empty();
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_scene_view().focus_on_selection(editor);
                    },
            },
            {
                .id = EditorActionId::ResetLayout,
                .label = "Reset Layout",
                .chord = ImGuiKey_None,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.request_reset_layout();
                    },
            },
            {
                .id = EditorActionId::RefreshAssets,
                .label = "Refresh Assets",
                .chord = ImGuiKey_None,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.get_assets().request_rebuild();
                    },
            },
            {
                .id = EditorActionId::NewScene,
                .label = "New Scene...",
                .chord = ImGuiMod_Ctrl | ImGuiKey_N,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return can_new_scene(editor);
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        show_new_scene_dialog(editor);
                    },
            },
            {
                .id = EditorActionId::OpenScene,
                .label = "Open Scene",
                .chord = ImGuiKey_None,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.open_pending_scene();
                    },
            },
            {
                .id = EditorActionId::Save,
                .label = "Save",
                .chord = ImGuiMod_Ctrl | ImGuiKey_S,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return can_save(editor);
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        save_all(editor);
                    },
            },
            {
                .id = EditorActionId::SaveSceneAs,
                .label = "Save Scene As...",
                .chord = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
                .context = EditorActionContext::Global,
                .is_enabled =
                    [](const Editor& editor) {
                        return can_save_scene_as(editor);
                    },
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        show_save_scene_as_dialog(editor);
                    },
            },
            {
                .id = EditorActionId::Quit,
                .label = "Quit",
                .chord = ImGuiKey_None,
                .context = EditorActionContext::Global,
                .is_enabled = nullptr,
                .format_label = nullptr,
                .run =
                    [](Editor& editor) {
                        editor.request_quit();
                    },
            },
        };

        static_assert(std::size(ACTIONS) == static_cast<size_t>(EditorActionId::Count));
    } // namespace

    const EditorAction& get_action(EditorActionId id) {
        const EditorAction& action = ACTIONS[static_cast<size_t>(id)];
        HOB_CHECK(action.id == id, "Editor action table is out of order at index {}", static_cast<int32_t>(id));

        return action;
    }

    std::span<const EditorAction> get_actions() {
        return std::span<const EditorAction>(ACTIONS);
    }

    uint32_t context_bit(EditorActionContext context) {
        return 1u << static_cast<uint32_t>(context);
    }

    bool is_action_enabled(const Editor& editor, EditorActionId id) {
        const EditorAction& action = get_action(id);
        return action.is_enabled == nullptr || action.is_enabled(editor);
    }

    bool is_action_active(const Editor& editor, EditorActionId id) {
        const EditorAction& action = get_action(id);
        return action.is_active != nullptr && action.is_active(editor);
    }

    std::string get_action_label(const Editor& editor, EditorActionId id) {
        const EditorAction& action = get_action(id);
        return action.format_label != nullptr ? action.format_label(editor) : std::string(action.label);
    }

    std::string make_command_label(const char* verb, const std::string& command_label) {
        return command_label.empty() ? std::string(verb) : std::string(verb) + " " + command_label;
    }

    std::string format_shortcut(ImGuiKeyChord chord) {
        const ImGuiKey key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
        if (key == ImGuiKey_None) {
            return std::string();
        }

        std::string text;
        if (chord & ImGuiMod_Ctrl) {
            text += "Ctrl+";
        }
        if (chord & ImGuiMod_Shift) {
            text += "Shift+";
        }
        if (chord & ImGuiMod_Alt) {
            text += "Alt+";
        }
        if (chord & ImGuiMod_Super) {
            text += "Super+";
        }

        text += ImGui::GetKeyName(key);

        return text;
    }

    bool is_chord_pressed(ImGuiKeyChord chord) {
        const ImGuiKey key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
        if (key == ImGuiKey_None) {
            return false;
        }

        // Exact match, so Ctrl+Shift+Z never fires the Ctrl+Z action.
        return ImGui::GetIO().KeyMods == (chord & ImGuiMod_Mask_) && ImGui::IsKeyPressed(key, false);
    }

    bool action_menu_item(Editor& editor, EditorActionId id) {
        const EditorAction& action = get_action(id);
        const std::string label = get_action_label(editor, id);
        const std::string shortcut = format_shortcut(action.chord);
        const bool enabled = is_action_enabled(editor, id);

        if (!menu_item(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), enabled)) {
            return false;
        }

        editor.request_action(id);

        return true;
    }

    bool action_bar_icon_button(Editor& editor,
                                EditorActionId id,
                                EditorBarIcon icon,
                                const EditorBarMetrics& metrics) {
        const EditorAction& action = get_action(id);
        const std::string shortcut = format_shortcut(action.chord);

        std::string tooltip = get_action_label(editor, id);
        if (!shortcut.empty()) {
            tooltip += " (" + shortcut + ")";
        }

        const char* button_id = action.label;
        if (!bar_icon_button(editor.get_icons(),
                             button_id,
                             icon,
                             is_action_enabled(editor, id),
                             is_action_active(editor, id),
                             tooltip.c_str(),
                             metrics)) {
            return false;
        }

        editor.request_action(id);

        return true;
    }
} // namespace hob::editor
