#include "editor_gui_utils.h"

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdarg>
#include <cstdio>
#include <format>

#include <imgui_internal.h>

#include "editor_style.h"
#include "engine/math/constants.h"
#include "engine/math/mathf.h"

namespace hob::editor {
    namespace {
        constexpr int32_t DRAW_CHANNEL_COUNT = 2;
        constexpr int32_t DRAW_CHANNEL_BACKGROUND = 0;
        constexpr int32_t DRAW_CHANNEL_FOREGROUND = 1;

        constexpr const char* COLOR_PICKER_POPUP_ID = "ColorPickerPopup";
        constexpr const char* COMBO_POPUP_ID = "##ComboPopup"; // The id ImGui hashes internally for a combo's popup.
        constexpr const char* BITMASK_POPUP_ID = "BitmaskPopup";
        constexpr const char* BITMASK_BUTTON_ID = "###BitmaskButton";
        constexpr const char* HEADER_MENU_BUTTON_ID = "###HeaderMenuButton";
        constexpr const char* BITMASK_SEPARATOR = " | ";
        constexpr uint32_t FIELD_STRING_CAPACITY = 256;

        // Words that read wrong title-cased. Matched whole, against a lowercase snake_case word.
        constexpr std::string_view LABEL_ACRONYMS[] = {"aabb"};

        template<std::totally_ordered T>
        ImGuiSliderFlags clamp_flags(T min, T max) {
            return min < max ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
        }

        char to_upper(char c) {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        void append_label_word(std::string& label, std::string_view word) {
            for (const std::string_view acronym : LABEL_ACRONYMS) {
                if (word == acronym) {
                    for (const char c : word) {
                        label += to_upper(c);
                    }

                    return;
                }
            }

            label += to_upper(word.front());
            label.append(word.substr(1));
        }

        ImRect inset_rect(const ImVec2& min, const ImVec2& max, const ImVec2& inset) {
            return ImRect(min.x + inset.x, min.y + inset.y, max.x - inset.x, max.y - inset.y);
        }

        // The row highlight spans the whole window, not just the indented item rect.
        ImRect row_rect(const ImVec2& inset) {
            const float window_min_x = ImGui::GetWindowPos().x;
            const float window_max_x = window_min_x + ImGui::GetWindowWidth();

            return inset_rect(ImVec2(window_min_x, ImGui::GetItemRectMin().y),
                              ImVec2(window_max_x, ImGui::GetItemRectMax().y),
                              inset);
        }

        void draw_highlight(ImDrawList* draw_list, const ImRect& rect, const ImVec4& color, float rounding) {
            draw_list->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(color), rounding);
        }

        void draw_bar_icon(
            ImDrawList* draw_list, const EditorIcons& icons, const ImRect& rect, EditorBarIcon icon, ImU32 color) {
            if (!icons.is_loaded()) {
                return;
            }

            const float size = static_cast<float>(ICON_SIZE_PX);
            const ImVec2 center(IM_ROUND(rect.GetCenter().x), IM_ROUND(rect.GetCenter().y));
            const float half = IM_ROUND(size * 0.5f);
            const ImVec2 min(center.x - half, center.y - half);

            draw_list->AddImage(icons.get_texture(),
                                min,
                                ImVec2(min.x + size, min.y + size),
                                icons.get_uv_min(icon),
                                icons.get_uv_max(icon),
                                color);
        }

        bool field_components(const char* label,
                              const ImVec4* colors,
                              float* const* values,
                              int32_t count,
                              float drag_speed,
                              float min,
                              float max) {
            begin_field(label);
            ImGui::BeginGroup();
            ImGui::PushMultiItemsWidths(count, ImGui::CalcItemWidth());

            bool changed = false;
            for (int32_t i = 0; i < count; ++i) {
                ImGui::PushID(i);
                if (i > 0) {
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                }

                ImGui::SetNextItemColorMarker(ImGui::GetColorU32(colors[i]));
                changed |= ImGui::DragFloat(INSPECTOR_EMPTY_LABEL,
                                            values[i],
                                            drag_speed,
                                            min,
                                            max,
                                            INSPECTOR_FLOAT_FORMAT,
                                            clamp_flags(min, max));

                ImGui::PopID();
                ImGui::PopItemWidth();
            }

            ImGui::EndGroup();
            end_field();

            return changed;
        }

    } // namespace

    std::string to_display_label(std::string_view name) {
        std::string label;
        label.reserve(name.size());

        for (size_t start = 0; start <= name.size();) {
            const size_t separator = name.find('_', start);
            const size_t end = (separator == std::string_view::npos) ? name.size() : separator;

            if (end > start) {
                if (!label.empty()) {
                    label += ' ';
                }

                append_label_word(label, name.substr(start, end - start));
            }

            if (separator == std::string_view::npos) {
                break;
            }

            start = separator + 1;
        }

        return label;
    }

    float get_bar_height() {
        return ImGui::GetCurrentWindow()->MenuBarRect().GetHeight();
    }

    ImGuiID dock_space_over_viewport(ImGuiDockNodeFlags flags) {
        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_ItemInnerSpacing, DOCK_TAB_SPACING);
        vars.push(ImGuiStyleVar_FramePadding, DOCK_TAB_PADDING);

        const ImGuiID dock_space_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), flags);

        vars.pop();

        return dock_space_id;
    }

    bool begin_dock(const char* name, ImGuiWindowFlags flags) {
        const ImGuiWindow* window = ImGui::FindWindowByName(name);
        const bool is_tab_selected = window && window->DockTabIsVisible;
        const bool is_floating = window && window->DockNode == nullptr;

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_TabHovered, is_tab_selected ? COLOR_BG_BASE : COLOR_BG_HOVER);
        colors.push(ImGuiCol_Border, is_floating ? COLOR_DOCK_BORDER_FLOATING : COLOR_DOCK_BORDER);

        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_WindowBorderSize, is_floating ? DOCK_BORDER_SIZE_FLOATING : DOCK_BORDER_SIZE);

        const bool visible = ImGui::Begin(name, nullptr, flags);

        vars.pop();
        colors.pop();

        return visible;
    }

    void end_dock() {
        ImGui::End();
    }

    bool begin_menu(const char* label) {
        const ImGuiStyle& style = ImGui::GetStyle();

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImDrawListSplitter splitter;
        splitter.Split(draw_list, DRAW_CHANNEL_COUNT);
        splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_FOREGROUND);

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_ItemSpacing, ImVec2(MENU_BAR_ITEM_PADDING.x * 2.0f, style.ItemSpacing.y));
        vars.push(ImGuiStyleVar_WindowPadding, BAR_POPUP_PADDING);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + BAR_ITEM_SPACING.x);
        const bool open = ImGui::BeginMenu(label);

        vars.pop();
        colors.pop();

        if (open || ImGui::IsItemHovered()) {
            splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_BACKGROUND);
            draw_highlight(draw_list,
                           ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()),
                           open ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER,
                           ROUNDING);
        }

        splitter.Merge(draw_list);

        return open;
    }

    void end_menu() {
        ImGui::EndMenu();
    }

    bool begin_submenu(const char* label, bool enabled) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        ImDrawList* draw_list = window->DrawList;
        ImDrawListSplitter splitter;
        splitter.Split(draw_list, DRAW_CHANNEL_COUNT);
        splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_FOREGROUND);

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_WindowPadding, BAR_POPUP_PADDING);

        const bool open = ImGui::BeginMenu(label, enabled);

        vars.pop();
        colors.pop();

        const bool hovered = !open && ImGui::IsItemHovered();
        if (enabled && (open || hovered)) {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float spacing_above = IM_TRUNC(style.ItemSpacing.y * 0.5f);
            const float row_top = window->DC.CursorPosPrevLine.y + window->DC.PrevLineTextBaseOffset - spacing_above;

            const ImVec2 row_min(window->Pos.x, row_top);
            const ImVec2 row_max(window->Pos.x + window->Size.x,
                                 row_top + window->DC.PrevLineSize.y + style.ItemSpacing.y);

            splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_BACKGROUND);
            draw_highlight(draw_list,
                           inset_rect(row_min, row_max, BAR_POPUP_ITEM_INSET),
                           open ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER,
                           ROUNDING);
        }

        splitter.Merge(draw_list);

        return open;
    }

    void end_submenu() {
        ImGui::EndMenu();
    }

    bool begin_context_menu(const char* str_id) {
        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_WindowPadding, BAR_POPUP_PADDING);

        const bool open = ImGui::BeginPopup(str_id);

        vars.pop();

        return open;
    }

    void end_context_menu() {
        ImGui::EndPopup();
    }

    bool menu_item(const char* label, const char* shortcut, bool enabled, bool selected) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImDrawListSplitter splitter;
        splitter.Split(draw_list, DRAW_CHANNEL_COUNT);
        splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_FOREGROUND);

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

        // ImGui draws the shortcut left-aligned in its own column, so hide it and redraw it flush to the right edge.
        colors.push(ImGuiCol_TextDisabled, COLOR_TRANSPARENT);

        const ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float shortcut_y = window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset;

        const bool pressed = ImGui::MenuItem(label, shortcut, selected, enabled);
        colors.pop();

        if (shortcut && shortcut[0]) {
            const ImGuiStyle& style = ImGui::GetStyle();
            ImVec4 color = style.Colors[ImGuiCol_TextDisabled];
            color.w *= enabled ? 1.0f : style.DisabledAlpha;

            const float shortcut_x = window->WorkRect.Max.x - ImGui::CalcTextSize(shortcut).x;
            draw_list->AddText(ImVec2(shortcut_x, shortcut_y), ImGui::GetColorU32(color), shortcut);
        }

        // A disabled row is inert, so it must not light up under the cursor either.
        if (enabled && (ImGui::IsItemActive() || ImGui::IsItemHovered())) {
            splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_BACKGROUND);
            draw_highlight(draw_list,
                           row_rect(BAR_POPUP_ITEM_INSET),
                           ImGui::IsItemActive() ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER,
                           ROUNDING);
        }

        splitter.Merge(draw_list);

        return pressed;
    }

    bool begin_combo(const char* preview) {
        const ImGuiID id = ImGui::GetID(INSPECTOR_EMPTY_LABEL);
        const bool open = ImGui::IsPopupOpen(ImHashStr(COMBO_POPUP_ID, 0, id), ImGuiPopupFlags_None);
        const bool held = ImGui::GetCurrentContext()->ActiveId == id;

        EditorStyleColorStack colors;
        if (open || held) {
            colors.push(ImGuiCol_FrameBg, COLOR_ITEM_ACTIVE);
            colors.push(ImGuiCol_FrameBgHovered, COLOR_ITEM_ACTIVE);
        }

        const bool opened = ImGui::BeginCombo(INSPECTOR_EMPTY_LABEL, preview);
        colors.pop();

        return opened;
    }

    void end_combo() {
        ImGui::EndCombo();
    }

    bool combo_item(const char* label, bool selected) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImDrawListSplitter splitter;
        splitter.Split(draw_list, DRAW_CHANNEL_COUNT);
        splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_FOREGROUND);

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

        const bool clicked = ImGui::Selectable(label, selected);
        colors.pop();

        if (selected || ImGui::IsItemHovered()) {
            splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_BACKGROUND);
            draw_highlight(draw_list,
                           ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()),
                           selected ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER,
                           ROUNDING);
        }

        splitter.Merge(draw_list);

        return clicked;
    }

    bool bar_icon_button(const EditorIcons& icons,
                         const char* id,
                         EditorBarIcon icon,
                         bool enabled,
                         bool active,
                         const char* tooltip,
                         const EditorBarMetrics& metrics) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        const ImRect bar_rect = window->MenuBarRect();
        const float button_height = metrics.button_height(bar_rect.GetHeight());
        const float padding_x = (metrics.button_width(bar_rect.GetHeight()) - ICON_SIZE_PX) * 0.5f;

        // Selectable adds CurrLineTextBaseOffset to its own Y, which BeginMenuBar seeds with
        // AlignTextToFramePadding; cancel it so the button sits where the inset asks.
        const float button_y = bar_rect.Min.y + metrics.button_inset.y - window->DC.CurrLineTextBaseOffset;

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
        colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_ItemSpacing, ImVec2(padding_x * 2.0f, 0.0f));

        ImGui::PushID(id);
        ImGui::BeginDisabled(!enabled);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + BAR_ITEM_SPACING.x, button_y));
        const bool pressed = ImGui::Selectable(
            INSPECTOR_EMPTY_LABEL, false, ImGuiSelectableFlags_None, ImVec2(ICON_SIZE_PX, button_height));
        ImGui::EndDisabled();
        ImGui::PopID();

        vars.pop();
        colors.pop();

        const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (ImGui::IsItemActive() || ImGui::IsItemHovered() || active) {
            const ImVec4& background = (ImGui::IsItemActive() || active) ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER;
            draw_highlight(draw_list, rect, background, ROUNDING);
        }

        const ImVec4& icon_color = active ? COLOR_BAR_ICON_ACTIVE : enabled ? COLOR_BAR_ICON : COLOR_BAR_ICON_DISABLED;
        draw_bar_icon(draw_list, icons, rect, icon, ImGui::GetColorU32(icon_color));

        if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            set_tooltip("%s", tooltip);
        }

        return pressed;
    }

    void set_tooltip(const char* fmt, ...) {
        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_WindowRounding, ROUNDING);

        va_list args;
        va_start(args, fmt);
        ImGui::SetTooltipV(fmt, args);
        va_end(args);

        vars.pop();
    }

    namespace {
        template<typename IdType>
        bool tree_item_v(IdType id, ImGuiTreeNodeFlags flags, bool selected, const char* fmt, va_list args) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImDrawListSplitter splitter;
            splitter.Split(draw_list, DRAW_CHANNEL_COUNT);
            splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_FOREGROUND);

            EditorStyleColorStack colors;
            colors.push(ImGuiCol_Header, COLOR_TRANSPARENT);
            colors.push(ImGuiCol_HeaderHovered, COLOR_TRANSPARENT);
            colors.push(ImGuiCol_HeaderActive, COLOR_TRANSPARENT);

            // A tree row is otherwise exactly as tall as its text, where a menu row is a Selectable
            // and carries ItemSpacing inside its own rect.
            EditorStyleVarStack vars;
            vars.push(ImGuiStyleVar_FramePadding, TREE_ITEM_PADDING);

            flags |= ImGuiTreeNodeFlags_FramePadding;

            if (selected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            const bool open = ImGui::TreeNodeExV(id, flags, fmt, args);

            vars.pop();
            colors.pop();

            if (selected || ImGui::IsItemHovered()) {
                splitter.SetCurrentChannel(draw_list, DRAW_CHANNEL_BACKGROUND);
                draw_highlight(
                    draw_list, row_rect(TREE_ITEM_INSET), selected ? COLOR_ITEM_ACTIVE : COLOR_ITEM_HOVER, ROUNDING);
            }

            splitter.Merge(draw_list);

            return open;
        }
    } // namespace

    void set_drag_payload(const char* type, const std::string& text) {
        ImGui::SetDragDropPayload(type, text.c_str(), text.size() + 1);
    }

    std::optional<std::string> accept_drag_payload(const char* type) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type);
        if (payload == nullptr) {
            return std::nullopt;
        }

        return std::string(static_cast<const char*>(payload->Data));
    }

    bool tree_item(const void* id, ImGuiTreeNodeFlags flags, bool selected, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        const bool open = tree_item_v(id, flags, selected, fmt, args);
        va_end(args);

        return open;
    }

    bool tree_item(const char* id, ImGuiTreeNodeFlags flags, bool selected, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        const bool open = tree_item_v(id, flags, selected, fmt, args);
        va_end(args);

        return open;
    }

    bool component_header(const char* label, bool* out_menu_requested) {
        EditorStyleColorStack colors;
        colors.push(ImGuiCol_Header, COLOR_INSPECTOR_HEADER);
        colors.push(ImGuiCol_HeaderHovered, COLOR_INSPECTOR_HEADER_HOVER);
        colors.push(ImGuiCol_HeaderActive, COLOR_INSPECTOR_HEADER_ACTIVE);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (out_menu_requested != nullptr) {
            flags |= ImGuiTreeNodeFlags_AllowOverlap;
        }

        const bool open = ImGui::CollapsingHeader(label, flags);

        colors.pop();

        if (out_menu_requested == nullptr) {
            return open;
        }

        *out_menu_requested = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        const ImVec2 header_min = ImGui::GetItemRectMin();
        const ImVec2 header_max = ImGui::GetItemRectMax();
        const float size = header_max.y - header_min.y;

        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(header_max.x - size, header_min.y));

        if (ImGui::InvisibleButton(HEADER_MENU_BUTTON_ID, ImVec2(size, size))) {
            *out_menu_requested = true;
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImRect button_rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
            draw_highlight(draw_list,
                           inset_rect(button_rect.Min, button_rect.Max, INSPECTOR_HEADER_MENU_INSET),
                           ImGui::IsItemActive() ? COLOR_INSPECTOR_HEADER_MENU_ACTIVE
                                                 : COLOR_INSPECTOR_HEADER_MENU_HOVER,
                           ROUNDING);
        }

        const ImVec2 center = button_rect.GetCenter();
        const ImU32 dot_color = ImGui::GetColorU32(ImGuiCol_Text);
        for (int32_t i = -1; i <= 1; ++i) {
            draw_list->AddCircleFilled(
                ImVec2(center.x, center.y + static_cast<float>(i) * INSPECTOR_HEADER_MENU_DOT_SPACING),
                INSPECTOR_HEADER_MENU_DOT_RADIUS,
                dot_color);
        }

        return open;
    }

    void begin_field(const char* label) {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        // The label may be indented (nested rows), so measure to its right edge, not its width.
        const float label_end_x = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
        const float column_x = std::max(INSPECTOR_LABEL_WIDTH, label_end_x + ITEM_SPACING.x);

        // SameLine adds the enclosing group's offset, which would push the rows nested inside a
        // composite field out of the column every other row sits in. Cancel it.
        ImGui::SameLine(column_x - ImGui::GetCurrentWindow()->DC.GroupOffset.x);
        ImGui::SetNextItemWidth(-EPSILON);
    }

    void end_field() {
        ImGui::PopID();
    }

    bool field_angle(const char* label, float& degrees, float drag_speed) {
        float normalized = math::normalize_angle_deg(degrees);
        float* components[] = {&normalized};
        const bool changed =
            field_components(label, &COLOR_AXIS_Z, components, IM_COUNTOF(components), drag_speed, 0.0f, 0.0f);

        if (changed) {
            degrees = normalized;
        }

        return changed;
    }

    bool field_float(const char* label, float& value, float drag_speed, float min, float max) {
        begin_field(label);
        const bool changed = ImGui::DragFloat(
            INSPECTOR_EMPTY_LABEL, &value, drag_speed, min, max, INSPECTOR_FLOAT_FORMAT, clamp_flags(min, max));
        end_field();

        return changed;
    }

    bool field_int(const char* label, int64_t& value, float drag_speed, int64_t min, int64_t max) {
        begin_field(label);
        const bool changed = ImGui::DragScalar(INSPECTOR_EMPTY_LABEL,
                                               ImGuiDataType_S64,
                                               &value,
                                               drag_speed,
                                               &min,
                                               &max,
                                               INSPECTOR_INT_FORMAT,
                                               clamp_flags(min, max));
        end_field();

        return changed;
    }

    bool field_bool(const char* label, bool& value) {
        begin_field(label);
        const bool changed = ImGui::Checkbox(INSPECTOR_EMPTY_LABEL, &value);
        end_field();

        return changed;
    }

    bool field_string(const char* label, std::string& value) {
        char buffer[FIELD_STRING_CAPACITY];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());

        begin_field(label);
        const bool changed = ImGui::InputText(INSPECTOR_EMPTY_LABEL, buffer, sizeof(buffer));
        end_field();

        if (changed) {
            value = buffer;
        }

        return changed;
    }

    void field_text(const char* label, const std::string& value) {
        char buffer[FIELD_STRING_CAPACITY];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());

        begin_field(label);
        ImGui::InputText(INSPECTOR_EMPTY_LABEL, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
        end_field();
    }

    bool field_vector2(const char* label, Vector2& value, float drag_speed) {
        const ImVec4 colors[] = {COLOR_AXIS_X, COLOR_AXIS_Y};
        float* components[] = {&value.x, &value.y};

        return field_components(label, colors, components, IM_COUNTOF(components), drag_speed, 0.0f, 0.0f);
    }

    bool field_vector3(const char* label, Vector3& value, float drag_speed) {
        const ImVec4 colors[] = {COLOR_AXIS_X, COLOR_AXIS_Y, COLOR_AXIS_Z};
        float* components[] = {&value.x, &value.y, &value.z};

        return field_components(label, colors, components, IM_COUNTOF(components), drag_speed, 0.0f, 0.0f);
    }

    bool field_euler_deg(const char* label, Vector3& degrees, float drag_speed) {
        Vector3 normalized(math::normalize_angle_deg(degrees.x),
                           math::normalize_angle_deg(degrees.y),
                           math::normalize_angle_deg(degrees.z));
        const bool changed = field_vector3(label, normalized, drag_speed);

        if (changed) {
            degrees = normalized;
        }

        return changed;
    }

    bool field_quaternion(const char* label, Quaternion& value) {
        Vector3 degrees = value.to_euler_deg();
        if (!field_euler_deg(label, degrees)) {
            return false;
        }

        value = Quaternion::from_euler_deg(degrees);
        return true;
    }

    bool field_color(const char* label, Color& value) {
        const ImVec4 colors[] = {COLOR_AXIS_X, COLOR_AXIS_Y, COLOR_AXIS_Z, COLOR_AXIS_W};
        float* components[] = {&value.r, &value.g, &value.b, &value.a};
        constexpr int32_t count = IM_COUNTOF(components);

        const ImGuiStyle& style = ImGui::GetStyle();
        const float swatch_width = ImGui::GetFrameHeight(); // ColorButton is square at its default size.

        begin_field(label);
        ImGui::BeginGroup();

        // The drags share the row with the swatch, so they split what is left of the field width.
        ImGui::PushMultiItemsWidths(count, ImGui::CalcItemWidth() - swatch_width - style.ItemInnerSpacing.x);

        bool changed = false;
        for (int32_t i = 0; i < count; ++i) {
            ImGui::PushID(i);
            if (i > 0) {
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            }

            ImGui::SetNextItemColorMarker(ImGui::GetColorU32(colors[i]));
            changed |= ImGui::DragFloat(INSPECTOR_EMPTY_LABEL,
                                        components[i],
                                        INSPECTOR_DRAG_SPEED_COLOR,
                                        0.0f,
                                        1.0f,
                                        INSPECTOR_FLOAT_FORMAT,
                                        ImGuiSliderFlags_AlwaysClamp);

            ImGui::PopID();
            ImGui::PopItemWidth();
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

        float rgba[4] = {value.r, value.g, value.b, value.a};
        if (ImGui::ColorButton(
                INSPECTOR_EMPTY_LABEL, ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]), INSPECTOR_COLOR_EDIT_FLAGS)) {
            ImGui::OpenPopup(COLOR_PICKER_POPUP_ID);
        }

        // Scoped by begin_field's PushID, so every color field gets its own popup.
        if (ImGui::BeginPopup(COLOR_PICKER_POPUP_ID)) {
            if (ImGui::ColorPicker4(INSPECTOR_EMPTY_LABEL, rgba, INSPECTOR_COLOR_EDIT_FLAGS)) {
                value = Color(rgba[0], rgba[1], rgba[2], rgba[3]);
                changed = true;
            }
            ImGui::EndPopup();
        }

        ImGui::EndGroup();
        end_field();

        return changed;
    }

    bool field_enum(const char* label, int64_t& value, const std::vector<EditorInspectorEntryEnum>& entries) {
        std::string name = std::to_string(value);
        for (const EditorInspectorEntryEnum& entry : entries) {
            if (entry.value == value) {
                name = entry.name;
                break;
            }
        }

        begin_field(label);

        bool changed = false;
        if (begin_combo(name.c_str())) {
            for (const EditorInspectorEntryEnum& entry : entries) {
                if (combo_item(entry.name.c_str(), entry.value == value) && entry.value != value) {
                    value = entry.value;
                    changed = true;
                }
            }

            end_combo();
        }

        end_field();

        return changed;
    }

    bool field_bitmask(const char* label, int64_t& value, const std::vector<EditorInspectorEntryEnum>& entries) {
        int64_t named_mask = 0;
        std::string named_summary;

        for (const EditorInspectorEntryEnum& entry : entries) {
            if (entry.value == 0) {
                continue;
            }

            named_mask |= entry.value;
            if ((value & entry.value) == entry.value) {
                if (!named_summary.empty()) {
                    named_summary += BITMASK_SEPARATOR;
                }

                named_summary += entry.name;
            }
        }

        const int64_t rest = value & ~named_mask;
        if (rest != 0) {
            if (!named_summary.empty()) {
                named_summary += BITMASK_SEPARATOR;
            }

            named_summary += std::format("0x{:x}", static_cast<uint64_t>(rest));
        }

        if (named_summary.empty()) {
            named_summary = INSPECTOR_NONE_LABEL;
        }

        begin_field(label);

        EditorStyleColorStack button_colors;
        if (ImGui::IsPopupOpen(BITMASK_POPUP_ID)) {
            button_colors.push(ImGuiCol_Button, COLOR_BUTTON_ACTIVE);
            button_colors.push(ImGuiCol_ButtonHovered, COLOR_BUTTON_ACTIVE);
        }

        const bool pressed =
            ImGui::Button((named_summary + BITMASK_BUTTON_ID).c_str(), ImVec2(ImGui::CalcItemWidth(), 0.0f));
        button_colors.pop();

        if (pressed) {
            ImGui::OpenPopup(BITMASK_POPUP_ID);
        }

        bool changed = false;

        if (ImGui::BeginPopup(BITMASK_POPUP_ID)) {
            for (const EditorInspectorEntryEnum& entry : entries) {
                if (entry.value == 0) {
                    continue;
                }

                bool set = (value & entry.value) == entry.value;
                if (ImGui::Checkbox(entry.name.c_str(), &set)) {
                    value ^= entry.value;
                    changed = true;
                }
            }

            ImGui::EndPopup();
        }

        end_field();

        return changed;
    }

    bool field_aabb(const char* label, AABB& value) {
        ImGui::PushID(label);
        ImGui::BeginGroup();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::Indent(INSPECTOR_NESTED_INDENT);
        bool changed = field_vector2("Center", value.center);
        changed |= field_vector2("Extents", value.extents);
        ImGui::Unindent(INSPECTOR_NESTED_INDENT);

        ImGui::EndGroup();
        ImGui::PopID();

        return changed;
    }

    bool field_aabb3(const char* label, AABB3& value) {
        ImGui::PushID(label);
        ImGui::BeginGroup();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::Indent(INSPECTOR_NESTED_INDENT);
        bool changed = field_vector3("Center", value.center);
        changed |= field_vector3("Extents", value.extents);
        ImGui::Unindent(INSPECTOR_NESTED_INDENT);

        ImGui::EndGroup();
        ImGui::PopID();

        return changed;
    }

    bool field_capsule(const char* label, Capsule& value) {
        ImGui::PushID(label);
        ImGui::BeginGroup();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::Indent(INSPECTOR_NESTED_INDENT);
        bool changed = field_vector2("Center A", value.center_a);
        changed |= field_vector2("Center B", value.center_b);
        changed |= field_float("Radius", value.radius, INSPECTOR_DRAG_SPEED_FLOAT, 0.0f, MAX_FLOAT);
        ImGui::Unindent(INSPECTOR_NESTED_INDENT);

        ImGui::EndGroup();
        ImGui::PopID();

        return changed;
    }

    bool field_circle(const char* label, Circle& value) {
        ImGui::PushID(label);
        ImGui::BeginGroup();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        ImGui::Indent(INSPECTOR_NESTED_INDENT);
        bool changed = field_vector2("Center", value.center);
        changed |= field_float("Radius", value.radius, INSPECTOR_DRAG_SPEED_FLOAT, 0.0f, MAX_FLOAT);
        ImGui::Unindent(INSPECTOR_NESTED_INDENT);

        ImGui::EndGroup();
        ImGui::PopID();

        return changed;
    }

    bool field_asset(const char* label,
                     const std::string& asset_name,
                     const std::string& display_name,
                     bool is_set,
                     const std::vector<EditorInspectorEntryAsset>& entries,
                     std::string& picked_asset_name) {
        begin_field(label);

        bool changed = false;
        if (begin_combo(display_name.c_str())) {
            if (combo_item(INSPECTOR_NONE_LABEL, !is_set) && is_set) {
                picked_asset_name.clear();
                changed = true;
            }

            for (int32_t i = 0; i < static_cast<int32_t>(entries.size()); ++i) {
                const EditorInspectorEntryAsset& entry = entries[i];
                const bool is_current = entry.name == asset_name;

                ImGui::PushID(i);
                if (combo_item(entry.name.c_str(), is_current) && !is_current) {
                    picked_asset_name = entry.name;
                    changed = true;
                }
                ImGui::PopID();
            }

            end_combo();
        }

        end_field();

        return changed;
    }
} // namespace hob::editor
