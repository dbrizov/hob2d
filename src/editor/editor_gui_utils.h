#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "editor_icons.h"
#include "editor_inspector_entries.h"
#include "editor_style.h"
#include "engine/math/aabb.h"
#include "engine/math/capsule.h"
#include "engine/math/circle.h"
#include "engine/math/color.h"
#include "engine/math/vector2.h"

namespace hob::editor {
    struct EditorStyleColorStack {
        int32_t count = 0;

        void push(ImGuiCol index, const ImVec4& color) {
            ImGui::PushStyleColor(index, color);
            count += 1;
        }

        void pop() {
            ImGui::PopStyleColor(count);
            count = 0;
        }
    };

    struct EditorStyleVarStack {
        int32_t count = 0;

        void push(ImGuiStyleVar index, float value) {
            ImGui::PushStyleVar(index, value);
            count += 1;
        }

        void push(ImGuiStyleVar index, const ImVec2& value) {
            ImGui::PushStyleVar(index, value);
            count += 1;
        }

        void pop() {
            ImGui::PopStyleVar(count);
            count = 0;
        }
    };

    inline ImVec2 to_imvec(const Vector2& v) {
        return ImVec2(v.x, v.y);
    }

    std::string to_display_label(std::string_view name);

    float get_bar_height();

    ImGuiID dock_space_over_viewport(ImGuiDockNodeFlags flags);

    bool begin_dock(const char* name, ImGuiWindowFlags flags = 0);
    void end_dock();

    bool begin_menu(const char* label);
    void end_menu();
    bool begin_submenu(const char* label, bool enabled = true);
    void end_submenu();
    bool menu_item(const char* label, const char* shortcut = nullptr, bool enabled = true, bool selected = false);
    bool begin_context_menu(const char* str_id);
    void end_context_menu();

    bool begin_combo(const char* preview);
    void end_combo();
    bool combo_item(const char* label, bool selected);

    bool bar_icon_button(const EditorIcons& icons,
                         const char* id,
                         EditorBarIcon icon,
                         bool enabled,
                         bool active,
                         const char* tooltip,
                         const EditorBarMetrics& metrics = BAR_METRICS);

    void set_tooltip(const char* fmt, ...) IM_FMTARGS(1);

    constexpr const char* DRAG_PAYLOAD_PREFAB = "hob.prefab";

    void set_drag_payload(const char* type, const std::string& text);
    std::optional<std::string> accept_drag_payload(const char* type);

    bool tree_item(const void* id, ImGuiTreeNodeFlags flags, bool selected, const char* fmt, ...) IM_FMTARGS(4);
    bool tree_item(const char* id, ImGuiTreeNodeFlags flags, bool selected, const char* fmt, ...) IM_FMTARGS(4);

    bool component_header(const char* label, bool* out_menu_requested = nullptr);

    void begin_field(const char* label);
    void end_field();
    bool field_angle(const char* label, float& degrees, float drag_speed = INSPECTOR_DRAG_SPEED_ROTATION_DEG);
    bool field_float(const char* label,
                     float& value,
                     float drag_speed = INSPECTOR_DRAG_SPEED_FLOAT,
                     float min = 0.0f,
                     float max = 0.0f);
    bool field_int(const char* label,
                   int64_t& value,
                   float drag_speed = INSPECTOR_DRAG_SPEED_INT,
                   int64_t min = 0,
                   int64_t max = 0);
    bool field_bool(const char* label, bool& value);
    bool field_string(const char* label, std::string& value);
    void field_text(const char* label, const std::string& value);
    bool field_vector2(const char* label, Vector2& value, float drag_speed = INSPECTOR_DRAG_SPEED_FLOAT);
    bool field_color(const char* label, Color& value);
    bool field_enum(const char* label, int64_t& value, const std::vector<EditorInspectorEntryEnum>& entries);
    bool field_bitmask(const char* label, int64_t& value, const std::vector<EditorInspectorEntryEnum>& entries);
    bool field_aabb(const char* label, AABB& value);
    bool field_capsule(const char* label, Capsule& value);
    bool field_circle(const char* label, Circle& value);
    bool field_asset(const char* label,
                     const std::string& asset_name,
                     const std::string& display_name,
                     bool is_set,
                     const std::vector<EditorInspectorEntryAsset>& entries,
                     std::string& picked_asset_name);
} // namespace hob::editor
