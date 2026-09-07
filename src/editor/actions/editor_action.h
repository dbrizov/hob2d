#pragma once

#include <span>
#include <string>

#include <imgui.h>

#include "editor/editor_style.h"

namespace hob::editor {
    class Editor;
    enum class EditorBarIcon : uint8_t;

    enum class EditorActionId : uint8_t {
        Undo,
        Redo,
        DuplicateSelection,
        DeleteSelection,
        Play,
        Pause,
        Step,
        Stop,
        GizmoTranslateRotate,
        GizmoTranslate,
        GizmoRotate,
        GizmoScale,
        GizmoToggleSpace,
        FocusSelection,
        ResetLayout,
        RefreshAssets,
        NewScene,
        OpenScene,
        Save,
        SaveSceneAs,
        NewPrefab,
        DeletePrefab,
        Quit,
        Count,
    };

    enum class EditorActionContext : uint8_t {
        Global,
        SceneView,
        Hierarchy,
        Inspector,
        Assets,
        Output,
        Count,
    };

    struct EditorAction {
        EditorActionId id;
        const char* label;
        ImGuiKeyChord chord; // ImGuiKey_None leaves the action unbound.
        EditorActionContext context;
        bool (*is_enabled)(const Editor&); // Null is always enabled.
        bool (*is_active)(const Editor&); // Null is never active.
        std::string (*format_label)(const Editor&); // Null uses label.
        void (*run)(Editor&);
    };

    const EditorAction& get_action(EditorActionId id);
    std::span<const EditorAction> get_actions();

    uint32_t context_bit(EditorActionContext context);

    bool is_action_enabled(const Editor& editor, EditorActionId id);
    bool is_action_active(const Editor& editor, EditorActionId id);
    std::string get_action_label(const Editor& editor, EditorActionId id);

    std::string make_command_label(const char* verb, const std::string& command_label);
    std::string format_shortcut(ImGuiKeyChord chord);
    bool is_chord_pressed(ImGuiKeyChord chord);

    bool action_menu_item(Editor& editor, EditorActionId id);
    bool action_bar_icon_button(Editor& editor,
                                EditorActionId id,
                                EditorBarIcon icon,
                                const EditorBarMetrics& metrics = BAR_METRICS);
} // namespace hob::editor
