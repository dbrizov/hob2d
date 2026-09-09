#include "editor_dock_audio_clips.h"

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr ImVec2 DEFAULT_SIZE = ImVec2(640.0f, 320.0f);
        constexpr ImGuiTableFlags TABLE_FLAGS =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    } // namespace

    EditorDockAudioClips::EditorDockAudioClips()
        : EditorDock("Audio Clips", EditorActionContext::Debug, false) {}

    void EditorDockAudioClips::draw(Editor& editor) {
        center_next_window(DEFAULT_SIZE);

        if (begin()) {
            const auto& clips = editor.get_engine().get_audio().get_clips();

            size_t live = 0;
            int32_t total_refs = 0;
            for (const auto& [path, weak] : clips) {
                if (auto clip = weak.lock()) {
                    // Subtract 1 for the strong ref held only for this iteration.
                    total_refs += static_cast<int32_t>(clip.use_count()) - 1;
                    live += 1;
                }
            }
            ImGui::Text("Clips: %zu | Refs: %d", live, total_refs);

            if (ImGui::BeginTable("clips", 2, TABLE_FLAGS)) {
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                for (const auto& [path, weak] : clips) {
                    auto clip = weak.lock();
                    if (!clip) {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", static_cast<int32_t>(clip.use_count()) - 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(clip->get_path().c_str());
                }
                ImGui::EndTable();
            }
        }
        end();
    }
} // namespace hob::editor
