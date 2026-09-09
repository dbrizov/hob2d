#include "editor_dock_sprite_queue.h"

#include <cstring>

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr ImVec2 DEFAULT_SIZE = ImVec2(900.0f, 420.0f);
        constexpr ImGuiTableFlags TABLE_FLAGS =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        const char* draw_path(const SpriteDrawData& draw) {
            return draw.texture ? draw.texture->get_path().c_str() : "<unknown>";
        }

        bool same_group(const SpriteDrawData& a, const SpriteDrawData& b) {
            return a.z_index == b.z_index && a.get_shader() == b.get_shader() &&
                   std::strcmp(draw_path(a), draw_path(b)) == 0;
        }
    } // namespace

    EditorDockSpriteQueue::EditorDockSpriteQueue()
        : EditorDock("Sprite Queue", EditorActionContext::Debug, false) {}

    void EditorDockSpriteQueue::draw(Editor& editor) {
        center_next_window(DEFAULT_SIZE);

        if (begin()) {
            // Entities destroyed earlier this frame have already popped their draws, so the order
            // is re-sorted here rather than left until the world pass, which runs after this.
            Renderer& renderer = editor.get_engine().get_renderer();
            renderer.sort_sprite_draws();

            const std::vector<SpriteDrawData>& draws = renderer.get_sprite_draws();
            const std::vector<uint32_t>& order = renderer.get_sprite_draw_order();

            // Collapse consecutive draws that share (z_index, shader, texture path) into a single row
            // with a count. Adjacent identical draws form one batch, so grouping by runs keeps the draw
            // order meaningful while cutting the row count.
            const auto run_end_from = [&](size_t start) {
                size_t run_end = start + 1;
                while (run_end < order.size() && same_group(draws[order[start]], draws[order[run_end]])) {
                    run_end += 1;
                }
                return run_end;
            };

            size_t group_count = 0;
            for (size_t i = 0; i < order.size(); i = run_end_from(i)) {
                group_count += 1;
            }

            ImGui::Text("Total: %zu draws | %zu groups", order.size(), group_count);

            if (ImGui::BeginTable("queue", 4, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Z Index", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < order.size();) {
                    const SpriteDrawData& draw = draws[order[i]];
                    const size_t run_end = run_end_from(i);
                    const Shader* shader = draw.get_shader();

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", run_end - i);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", draw.z_index);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(shader ? shader->get_debug_label().c_str() : "<none>");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(draw_path(draw));

                    i = run_end;
                }
                ImGui::EndTable();
            }
        }
        end();
    }
} // namespace hob::editor
