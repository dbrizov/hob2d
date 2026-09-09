#include "editor_dock_materials.h"

#include <format>
#include <string>

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr ImVec2 DEFAULT_SIZE = ImVec2(900.0f, 420.0f);
        constexpr ImGuiTableFlags TABLE_FLAGS =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        std::string shader_label(const Shader* shader) {
            return shader != nullptr ? shader->get_debug_label() : "<none>";
        }

        std::string material_textures_label(const Material& material) {
            const Shader* shader = material.get_shader();
            if (shader == nullptr || shader->get_textures().empty()) {
                return "-";
            }

            std::string label;
            for (const ShaderTexture& shader_texture : shader->get_textures()) {
                const TextureRef& texture = material.get_texture(shader_texture.name);
                if (!label.empty()) {
                    label += ", ";
                }
                label += std::format("{}={}", shader_texture.name, texture ? texture->get_path() : "<none>");
            }
            return label;
        }

        // The lock held for the current iteration is a ref of its own (+1).
        int32_t ref_count(const MaterialRef& material) {
            return static_cast<int32_t>(material.use_count()) - 1;
        }
    } // namespace

    EditorDockMaterials::EditorDockMaterials()
        : EditorDock("Materials", EditorActionContext::Debug, false) {}

    void EditorDockMaterials::draw(Editor& editor) {
        center_next_window(DEFAULT_SIZE);

        if (begin()) {
            const auto& materials = editor.get_engine().get_renderer().get_materials();

            size_t live = 0;
            int32_t total_refs = 0;
            for (const auto& weak : materials) {
                if (auto material = weak.lock()) {
                    total_refs += ref_count(material);
                    live += 1;
                }
            }
            ImGui::Text("Materials: %zu | Refs: %d", live, total_refs);

            if (ImGui::BeginTable("materials", 4, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Shader", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Textures", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& weak : materials) {
                    auto material = weak.lock();
                    if (!material) {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(material->get_name().empty() ? "<inline>" : material->get_name().c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", ref_count(material));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(shader_label(material->get_shader()).c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(material_textures_label(*material).c_str());
                }
                ImGui::EndTable();
            }
        }
        end();
    }
} // namespace hob::editor
