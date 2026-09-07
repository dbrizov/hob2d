#include "editor_dock_hierarchy.h"

#include <string>
#include <vector>

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_style.h"
#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"

namespace hob::editor {
    EditorDockHierarchy::EditorDockHierarchy()
        : EditorDock("Hierarchy", EditorActionContext::Hierarchy) {}

    void EditorDockHierarchy::draw(Editor& editor) {
        if (begin()) {
            std::vector<Entity*> entities;
            editor.get_engine().get_entity_spawner().get_entities(entities);

            ImGui::Text("Entities: %zu", entities.size());
            ImGui::Separator();

            // Only entities actually drawn land here, so a range select spans what the user can see.
            std::vector<EntityId> visible_order;
            visible_order.reserve(entities.size());

            EntityId clicked_entity_id = INVALID_ENTITY_ID;

            EditorStyleVarStack vars;
            vars.push(ImGuiStyleVar_ItemSpacing, TREE_ITEM_SPACING);

            // Draw parentless entities. Child entities are drawn recursively
            for (const Entity* entity : entities) {
                const TransformComponent* transform = entity->get_transform();
                if (transform->get_parent() != nullptr) {
                    continue;
                }

                draw_entity(editor, transform, visible_order, clicked_entity_id);
            }

            vars.pop();

            if (clicked_entity_id != INVALID_ENTITY_ID) {
                const ImGuiIO& io = ImGui::GetIO();
                editor.get_selection().apply_click(EditorSelectionClick{clicked_entity_id, io.KeyCtrl, io.KeyShift},
                                                   visible_order);
            }

            m_scroll_to_primary = false;
        }
        end();
    }

    void EditorDockHierarchy::scroll_to_primary() {
        m_scroll_to_primary = true;
    }

    void EditorDockHierarchy::draw_entity(const Editor& editor,
                                          const TransformComponent* transform,
                                          std::vector<EntityId>& visible_order,
                                          EntityId& out_clicked_entity_id) {
        const Entity& entity = transform->get_entity();
        const EntityId entity_id = entity.get_id();
        const std::vector<TransformComponent*>& children = transform->get_children();
        const EditorSelection& selection = editor.get_selection();

        visible_order.push_back(entity_id);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        const bool open = tree_item(reinterpret_cast<void*>(static_cast<intptr_t>(entity_id)),
                                    flags,
                                    selection.contains(entity_id),
                                    "%s  #%lld",
                                    entity.get_display_name().c_str(),
                                    static_cast<long long>(entity_id));

        if (m_scroll_to_primary && entity_id == selection.primary()) {
            ImGui::SetScrollHereY(0.5f);
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            out_clicked_entity_id = entity_id;
        }

        if (ImGui::BeginDragDropSource()) {
            set_drag_payload(DRAG_PAYLOAD_ENTITY, std::to_string(entity_id));
            ImGui::TextUnformatted(entity.get_display_name().c_str());
            ImGui::EndDragDropSource();
        }

        if (open) {
            for (const TransformComponent* child : children) {
                draw_entity(editor, child, visible_order, out_clicked_entity_id);
            }
            ImGui::TreePop();
        }
    }

} // namespace hob::editor
