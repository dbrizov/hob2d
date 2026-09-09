#include "editor_dock.h"

#include <format>

#include "editor/editor_gui_utils.h"

namespace hob::editor {
    EditorDock::EditorDock(std::string_view id, EditorActionContext context, bool visible)
        : m_id(id)
        , m_label(id)
        , m_context(context)
        , m_visible(visible) {
        update_name();
    }

    void EditorDock::set_label(std::string_view label) {
        m_label = label;
        update_name();
    }

    bool EditorDock::begin(ImGuiWindowFlags flags) {
        m_hovered = false;
        m_focused = false;

        const bool visible = begin_dock(m_name.c_str(), &m_visible, flags);
        if (visible) {
            m_hovered = ImGui::IsWindowHovered();
            m_focused = ImGui::IsWindowFocused();
        }

        return visible;
    }

    void EditorDock::end() {
        end_dock();
    }

    const std::string& EditorDock::get_id() const {
        return m_id;
    }

    const std::string& EditorDock::get_name() const {
        return m_name;
    }

    EditorActionContext EditorDock::get_context() const {
        return m_context;
    }

    bool EditorDock::is_visible() const {
        return m_visible;
    }

    void EditorDock::set_visible(bool visible) {
        m_visible = visible;
    }

    bool EditorDock::is_hovered() const {
        return m_hovered;
    }

    bool EditorDock::is_focused() const {
        return m_focused;
    }

    void EditorDock::update_name() {
        m_name = std::format(" {} ###{}", m_label, m_id);
    }
} // namespace hob::editor
