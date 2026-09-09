#include "editor_dock_textures.h"

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr ImVec2 DEFAULT_SIZE = ImVec2(760.0f, 420.0f);
        constexpr ImGuiTableFlags TABLE_FLAGS =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    } // namespace

    EditorDockTextures::EditorDockTextures()
        : EditorDock("Textures", EditorActionContext::Debug, false) {}

    void EditorDockTextures::draw(Editor& editor) {
        center_next_window(DEFAULT_SIZE);

        if (begin()) {
            const Renderer& renderer = editor.get_engine().get_renderer();
            const auto& textures = renderer.get_textures();

            int32_t total_refs = 0;
            for (const auto& [path, weak] : textures) {
                if (auto texture = weak.lock()) {
                    // Subtract 1 because `texture` itself is a strong ref held only for this iteration.
                    total_refs += static_cast<int32_t>(texture.use_count()) - 1;
                }
            }
            ImGui::Text("Textures: %zu | Refs: %d", textures.size(), total_refs);

            if (ImGui::BeginTable("textures", 5, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Filter", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Wrap", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& [path, weak] : textures) {
                    auto texture = weak.lock();
                    if (!texture) {
                        continue;
                    }

                    // A texture with no explicit sampler is drawn with the engine default sampler.
                    const SamplerDesc& sampler =
                        texture->get_sampler() ? texture->get_sampler_desc() : renderer.get_default_sampler_desc();

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%ux%u", texture->get_width(), texture->get_height());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", static_cast<int32_t>(texture.use_count()) - 1);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(texture_filter_to_string(sampler.filter));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(texture_wrap_to_string(sampler.wrap));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(texture->get_path().c_str());
                }
                ImGui::EndTable();
            }
        }
        end();
    }
} // namespace hob::editor
