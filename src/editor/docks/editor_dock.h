#pragma once

#include <string>
#include <string_view>

#include <imgui.h>

#include "editor/actions/editor_action.h"

namespace hob::editor {
    class Editor;

    class EditorDock {
        std::string m_id;
        std::string m_label;
        std::string m_name; // The actual key name used for the ImGui widget
        EditorActionContext m_context;
        bool m_visible;

    protected:
        bool m_hovered = false;
        bool m_focused = false;

        void set_label(std::string_view label);

        bool begin(ImGuiWindowFlags flags = 0);
        void end();

    public:
        EditorDock(std::string_view id, EditorActionContext context, bool visible = true);
        virtual ~EditorDock() = default;

        EditorDock(const EditorDock&) = delete;
        EditorDock& operator=(const EditorDock&) = delete;

        EditorDock(EditorDock&&) = delete;
        EditorDock& operator=(EditorDock&&) = delete;

        virtual void draw(Editor& editor) = 0;

        const std::string& get_id() const;
        const std::string& get_name() const;
        EditorActionContext get_context() const;

        bool is_visible() const;
        void set_visible(bool visible);

        bool is_hovered() const;
        bool is_focused() const;

    private:
        void update_name();
        void draw_dock_menu();
    };
} // namespace hob::editor
