#pragma once

#include <vector>

#include "editor/editor_camera_3d.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_instance_id.h"
#include "engine/entity/entity.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/vector3.h"

struct ImDrawList;

namespace hob::editor {
    class Editor;

    class EditorGizmo3D {
        struct DragEntity {
            EntityId entity_id = INVALID_ENTITY_ID;
            EditorInstanceId instance_id = INVALID_EDITOR_INSTANCE_ID;
            Vector3 start_local_position;
            Vector3 start_local_euler_deg;
            Vector3 start_local_scale;
            Matrix4x4 start_world = Matrix4x4::identity();
        };

        EditorGizmoMode m_mode = EditorGizmoMode::TranslateRotate;
        EditorGizmoSpace m_space = EditorGizmoSpace::World;
        bool m_using = false;
        std::vector<DragEntity> m_drag_entities;

    public:
        EditorGizmoMode get_mode() const;
        void set_mode(EditorGizmoMode mode);

        EditorGizmoSpace get_space() const;
        void toggle_space();

        bool is_using() const;
        bool is_hovered() const;
        void reset();

        void update_and_draw(Editor& editor,
                             ImDrawList* draw_list,
                             const EditorCamera3D& camera,
                             const EditorSceneRect& scene_rect);

    private:
        void begin_drag(Editor& editor);
        void apply_drag(Editor& editor, const Matrix4x4& primary_world);
        void end_drag(Editor& editor);
    };
} // namespace hob::editor
