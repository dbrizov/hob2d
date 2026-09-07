#pragma once

#include <vector>

#include "editor/editor_camera.h"
#include "editor/editor_instance_id.h"
#include "engine/entity/entity.h"
#include "engine/math/vector2.h"

struct ImDrawList;

namespace hob::editor {
    class Editor;

    enum class EditorGizmoMode : uint8_t {
        TranslateRotate,
        Translate,
        Rotate,
        Scale,
    };

    enum class EditorGizmoSpace : uint8_t {
        World,
        Local,
    };

    enum class EditorGizmoHandle : uint8_t {
        None,
        AxisX,
        AxisY,
        Plane,
        Ring,
        Uniform,
    };

    class EditorGizmo {
        struct Frame {
            bool valid = false;
            Vector2 pivot_world;
            Vector2 pivot_screen;
            Vector2 axis_x_screen;
            Vector2 axis_y_screen;
            Vector2 axis_x_world;
            Vector2 axis_y_world;
        };

        struct DragEntity {
            EntityId entity_id = INVALID_ENTITY_ID;
            EditorInstanceId instance_id = INVALID_EDITOR_INSTANCE_ID;
            Vector2 start_local_position;
            float start_local_rotation = 0.0f;
            Vector2 start_local_scale;
            Vector2 start_world_position;
        };

        EditorGizmoMode m_mode = EditorGizmoMode::TranslateRotate;
        EditorGizmoSpace m_space = EditorGizmoSpace::World;

        EditorGizmoHandle m_hovered_handle = EditorGizmoHandle::None;

        EditorGizmoHandle m_dragged_handle = EditorGizmoHandle::None;
        std::vector<DragEntity> m_drag_entities;
        Vector2 m_drag_pivot_world;
        Vector2 m_drag_axis_x_world;
        Vector2 m_drag_axis_y_world;
        Vector2 m_drag_grab_world;
        float m_drag_pixels_per_meter = 1.0f;
        float m_drag_previous_angle = 0.0f;
        float m_drag_total_rotation = 0.0f;

    public:
        EditorGizmoMode get_mode() const;
        void set_mode(EditorGizmoMode mode);

        EditorGizmoSpace get_space() const;
        void set_space(EditorGizmoSpace space);
        void toggle_space();

        bool is_dragging() const;

        bool update_input(Editor& editor,
                          const Vector2& mouse_screen_position,
                          const EditorCamera& camera,
                          const EditorSceneRect& scene_rect);

        void draw(const Editor& editor,
                  ImDrawList* draw_list,
                  const EditorCamera& camera,
                  const EditorSceneRect& scene_rect) const;

        void clear_hover();
        void reset();

    private:
        Frame build_frame(const Editor& editor, const EditorCamera& camera, const EditorSceneRect& scene_rect) const;

        static Vector2 get_composite_center(const Frame& frame);

        void draw_translate_handles(ImDrawList* draw_list, const Frame& frame, EditorGizmoHandle active_handle) const;
        void draw_rotate_ring(ImDrawList* draw_list, const Frame& frame, EditorGizmoHandle active_handle) const;
        void draw_scale_handles(ImDrawList* draw_list, const Frame& frame, EditorGizmoHandle active_handle) const;

        EditorGizmoHandle pick_handle(const Frame& frame, const Vector2& mouse_screen_position) const;

        static EditorGizmoHandle pick_axis_handle(const Frame& frame, const Vector2& mouse_screen_position);
        static bool is_on_ring(const Frame& frame, const Vector2& mouse_screen_position);

        void begin_drag(const Editor& editor,
                        const Frame& frame,
                        EditorGizmoHandle handle,
                        const Vector2& mouse_world_position,
                        const EditorCamera& camera);
        void update_drag(Editor& editor, const Vector2& mouse_world_position);
        void end_drag(Editor& editor);
    };
} // namespace hob::editor
