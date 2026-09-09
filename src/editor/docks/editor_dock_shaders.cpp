#include "editor_dock_shaders.h"

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr ImVec2 DEFAULT_SIZE = ImVec2(720.0f, 360.0f);
        constexpr ImGuiTableFlags TABLE_FLAGS =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    } // namespace

    EditorDockShaders::EditorDockShaders()
        : EditorDock("Shaders", EditorActionContext::Debug, false) {}

    void EditorDockShaders::draw(Editor& editor) {
        center_next_window(DEFAULT_SIZE);

        if (begin()) {
            const auto& shaders = editor.get_engine().get_renderer().get_shaders();
            ImGui::Text("Shaders: %zu", shaders.size());

            if (ImGui::BeginTable("shaders", 2, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& [key, shader] : shaders) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    // Subtract 1 for the map's own ref, so the count shows external holders.
                    ImGui::Text("%d", static_cast<int32_t>(shader.use_count()) - 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(shader->get_debug_label().c_str());
                }
                ImGui::EndTable();
            }
        }
        end();
    }
} // namespace hob::editor
