#include "editor_gizmo_3d.h"

#include <memory>
#include <utility>

#include <ImGuizmo.h>
#include <imgui.h>
#include <sol/sol.hpp>

#include "editor/commands/editor_command_composite.h"
#include "editor/editor.h"
#include "editor/editor_gizmo_fields.h"
#include "editor/editor_instances.h"
#include "editor/editor_lua.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"
#include "engine/core/systems/scripting/lua_script_system.h"

namespace hob::editor {
    namespace {
        constexpr const char* LABEL_TRANSLATE = "Translate";
        constexpr const char* LABEL_ROTATE = "Rotate";
        constexpr const char* LABEL_SCALE = "Scale";
        constexpr float SNAP_TRANSLATE_METERS = 1.0f;
        constexpr float SNAP_ROTATE_DEG = 15.0f;
        constexpr float SNAP_SCALE = 0.1f;

        ImGuizmo::OPERATION to_operation(EditorGizmoMode mode) {
            switch (mode) {
                case EditorGizmoMode::Translate:
                    return ImGuizmo::TRANSLATE;
                case EditorGizmoMode::Rotate:
                    return ImGuizmo::ROTATE;
                case EditorGizmoMode::Scale:
                    return ImGuizmo::SCALE;
                default:
                    return static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE | ImGuizmo::ROTATE);
            }
        }

        const float* snap_for(EditorGizmoMode mode, bool snapping) {
            static const float translate[3] = {SNAP_TRANSLATE_METERS, SNAP_TRANSLATE_METERS, SNAP_TRANSLATE_METERS};
            static const float rotate[3] = {SNAP_ROTATE_DEG, SNAP_ROTATE_DEG, SNAP_ROTATE_DEG};
            static const float scale[3] = {SNAP_SCALE, SNAP_SCALE, SNAP_SCALE};
            if (!snapping) {
                return nullptr;
            }

            switch (mode) {
                case EditorGizmoMode::Rotate:
                    return rotate;
                case EditorGizmoMode::Scale:
                    return scale;
                default:
                    return translate;
            }
        }

        const char* label_for(EditorGizmoMode mode, const Matrix4x4& delta) {
            switch (mode) {
                case EditorGizmoMode::Rotate:
                    return LABEL_ROTATE;
                case EditorGizmoMode::Scale:
                    return LABEL_SCALE;
                case EditorGizmoMode::Translate:
                    return LABEL_TRANSLATE;
                default:
                    return delta.get_rotation() == Quaternion::identity() ? LABEL_TRANSLATE : LABEL_ROTATE;
            }
        }

        Matrix4x4 parent_world_of(const TransformComponent3D& transform) {
            return transform.get_parent() != nullptr ? transform.get_parent()->get_world_matrix()
                                                     : Matrix4x4::identity();
        }
    } // namespace

    EditorGizmoMode EditorGizmo3D::get_mode() const {
        return m_mode;
    }

    void EditorGizmo3D::set_mode(EditorGizmoMode mode) {
        m_mode = mode;
    }

    EditorGizmoSpace EditorGizmo3D::get_space() const {
        return m_space;
    }

    void EditorGizmo3D::toggle_space() {
        m_space = (m_space == EditorGizmoSpace::World) ? EditorGizmoSpace::Local : EditorGizmoSpace::World;
    }

    bool EditorGizmo3D::is_using() const {
        return m_using;
    }

    bool EditorGizmo3D::is_hovered() const {
        return ImGuizmo::IsOver();
    }

    void EditorGizmo3D::reset() {
        m_using = false;
        m_drag_entities.clear();
    }

    void EditorGizmo3D::update_and_draw(Editor& editor,
                                        ImDrawList* draw_list,
                                        const EditorCamera3D& camera,
                                        const EditorSceneRect& scene_rect) {
        const EditorSelection& selection = editor.get_selection();
        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();
        Entity* primary = spawner.get_entity(selection.primary());
        TransformComponent3D* transform = primary != nullptr ? primary->get_transform_3d() : nullptr;
        if (transform == nullptr || !can_edit_selected_instances(editor)) {
            if (m_using) {
                end_drag(editor);
            }
            return;
        }

        ImGuizmo::SetDrawlist(draw_list);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(scene_rect.top_left.x, scene_rect.top_left.y, scene_rect.size.x, scene_rect.size.y);

        const Matrix4x4 view = camera.build_view_matrix();
        const Matrix4x4 projection = camera.build_display_projection(scene_rect.size);
        Matrix4x4 world = transform->get_world_matrix();

        const ImGuizmo::MODE mode = (m_space == EditorGizmoSpace::Local || m_mode == EditorGizmoMode::Scale)
                                        ? ImGuizmo::LOCAL
                                        : ImGuizmo::WORLD;

        ImGuizmo::Manipulate(view.data(),
                             projection.data(),
                             to_operation(m_mode),
                             mode,
                             world.data(),
                             nullptr,
                             snap_for(m_mode, ImGui::GetIO().KeyCtrl));

        const bool using_now = ImGuizmo::IsUsing();
        if (using_now && !m_using) {
            begin_drag(editor);
        }

        if (using_now) {
            apply_drag(editor, world);
        }
        else if (m_using) {
            end_drag(editor);
        }

        m_using = using_now;
    }

    void EditorGizmo3D::begin_drag(Editor& editor) {
        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();

        m_drag_entities.clear();
        for (EntityId id : editor.get_selection().ids) {
            const Entity* entity = spawner.get_entity(id);
            const TransformComponent3D* transform = entity != nullptr ? entity->get_transform_3d() : nullptr;
            if (transform == nullptr) {
                continue;
            }

            m_drag_entities.push_back(DragEntity{
                .entity_id = id,
                .instance_id = get_instance_id_of_entity(editor.get_engine(), id),
                .start_local_position = transform->get_local_position(),
                .start_local_euler_deg = transform->get_local_euler_deg(),
                .start_local_scale = transform->get_local_scale(),
                .start_world = transform->get_world_matrix(),
            });
        }
    }

    void EditorGizmo3D::apply_drag(Editor& editor, const Matrix4x4& primary_world) {
        if (m_drag_entities.empty()) {
            return;
        }

        Engine& engine = editor.get_engine();
        const EntitySpawner& spawner = engine.get_entity_spawner();
        sol::state& lua = engine.get_lua_script_system().get_lua();

        const DragEntity& primary = m_drag_entities.back();
        const Matrix4x4 delta = primary_world * primary.start_world.inverse();
        const bool scale_only = m_mode == EditorGizmoMode::Scale;

        for (const DragEntity& drag : m_drag_entities) {
            Entity* entity = spawner.get_entity(drag.entity_id);
            TransformComponent3D* transform = entity != nullptr ? entity->get_transform_3d() : nullptr;
            if (transform == nullptr) {
                continue;
            }

            const bool is_primary = drag.entity_id == primary.entity_id;
            if (scale_only && !is_primary) {
                continue;
            }

            const Matrix4x4 new_world = is_primary ? primary_world : delta * drag.start_world;
            const Matrix4x4 new_local = parent_world_of(*transform).inverse() * new_world;

            Vector3 position;
            Quaternion rotation;
            Vector3 scale;
            new_local.decompose(position, rotation, scale);

            const auto target = [&](const char* field) {
                return make_transform_target(drag.entity_id, drag.instance_id, transform_3d_key::SECTION, field);
            };

            if (scale_only) {
                try_set_transform_field(editor, lua, target(transform_key::SCALE), transform->get_local_scale(), scale);
                continue;
            }

            try_set_transform_field(
                editor, lua, target(transform_key::POSITION), transform->get_local_position(), position);

            if (m_mode != EditorGizmoMode::Translate) {
                try_set_transform_field(editor,
                                        lua,
                                        target(transform_key::ROTATION),
                                        transform->get_local_euler_deg(),
                                        rotation.to_euler_deg());
            }
        }
    }

    void EditorGizmo3D::end_drag(Editor& editor) {
        Engine& engine = editor.get_engine();
        const EntitySpawner& spawner = engine.get_entity_spawner();
        sol::state& lua = engine.get_lua_script_system().get_lua();

        Matrix4x4 delta = Matrix4x4::identity();
        if (!m_drag_entities.empty()) {
            const DragEntity& primary = m_drag_entities.back();
            if (const Entity* entity = spawner.get_entity(primary.entity_id)) {
                if (const TransformComponent3D* transform = entity->get_transform_3d()) {
                    delta = transform->get_world_matrix() * primary.start_world.inverse();
                }
            }
        }
        const char* label = label_for(m_mode, delta);

        std::vector<std::unique_ptr<EditorCommand>> commands;
        for (const DragEntity& drag : m_drag_entities) {
            const Entity* entity = spawner.get_entity(drag.entity_id);
            const TransformComponent3D* transform = entity != nullptr ? entity->get_transform_3d() : nullptr;
            if (transform == nullptr) {
                continue;
            }

            const auto target = [&](const char* field) {
                return make_transform_target(drag.entity_id, drag.instance_id, transform_3d_key::SECTION, field);
            };

            push_transform_command_if_changed(commands,
                                              lua,
                                              label,
                                              target(transform_key::POSITION),
                                              drag.start_local_position,
                                              transform->get_local_position());
            push_transform_command_if_changed(commands,
                                              lua,
                                              label,
                                              target(transform_key::ROTATION),
                                              drag.start_local_euler_deg,
                                              transform->get_local_euler_deg());
            push_transform_command_if_changed(commands,
                                              lua,
                                              label,
                                              target(transform_key::SCALE),
                                              drag.start_local_scale,
                                              transform->get_local_scale());
        }

        m_drag_entities.clear();
        m_using = false;

        if (commands.empty()) {
            return;
        }

        std::unique_ptr<EditorCommand> command =
            (commands.size() == 1) ? std::move(commands.front())
                                   : std::make_unique<EditorCommandComposite>(label, std::move(commands));

        editor.get_commands().push(editor, std::move(command));
    }
} // namespace hob::editor
