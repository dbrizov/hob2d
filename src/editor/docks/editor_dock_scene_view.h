#pragma once

#include <vector>

#include <SDL3/SDL_gpu.h>

#include "editor/editor_camera.h"
#include "editor/editor_camera_3d.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_gizmo_3d.h"
#include "editor_dock.h"
#include "engine/core/systems/renderer/render_targets_3d.h"
#include "engine/entity/entity.h"
#include "engine/math/ray.h"
#include "engine/math/vector2.h"

struct ImDrawList;

namespace hob::editor {
    class EditorDockSceneView : public EditorDock {
        EditorCamera m_camera;
        EditorCamera3D m_camera_3d;
        EditorGizmo m_gizmo;
        EditorGizmo3D m_gizmo_3d;
        bool m_flying = false;

        SDL_GPUTexture* m_color_target = nullptr;
        RenderTargets3D m_targets_3d;
        uint32_t m_color_target_width = 0;
        uint32_t m_color_target_height = 0;

        // Recorded while drawing, consumed by the next frame's input phase.
        EditorSceneRect m_rect;
        bool m_rect_valid = false;

        // Clicking the same spot repeatedly cycles through overlapping candidates.
        Vector2 m_pick_cycle_screen_position;
        EntityId m_pick_cycle_last_entity_id = INVALID_ENTITY_ID;

    public:
        EditorDockSceneView();

        void update_input(Editor& editor);
        void draw(Editor& editor) override;
        void render_pass(Editor& editor);
        void release_color_target(Editor& editor);

        EditorGizmoMode get_gizmo_mode() const;
        void set_gizmo_mode(EditorGizmoMode mode);
        EditorGizmoSpace get_gizmo_space() const;
        void toggle_gizmo_space();
        void reset_gizmo();

        void focus_on_selection(const Editor& editor);
        void reset_pick_cycle();
        bool is_flying() const;

    private:
        void ensure_color_target(Editor& editor, uint32_t width, uint32_t height);

        void handle_pick(Editor& editor, const Vector2& mouse_screen_pos, const Vector2& mouse_world_pos);
        void gather_pick_candidates(const Editor& editor,
                                    const Vector2& world_pos,
                                    std::vector<EntityId>& out_candidates) const;

        void handle_prefab_drop(Editor& editor, const EditorSceneRect& scene_rect);

        void draw_toolbar(Editor& editor);
        void draw_grid(ImDrawList* draw_list, const EditorSceneRect& scene_rect) const;

        void update_input_3d(Editor& editor);
        void draw_3d(Editor& editor, ImDrawList* draw_list, const EditorSceneRect& scene_rect);
        void render_pass_3d(Editor& editor);
        void focus_on_selection_3d(const Editor& editor);
        void handle_pick_3d(Editor& editor, const Vector2& mouse_screen_pos);
        void gather_pick_candidates_3d(const Editor& editor,
                                       const Ray& ray,
                                       std::vector<EntityId>& out_candidates) const;
        void handle_prefab_drop_3d(Editor& editor, const EditorSceneRect& scene_rect);
        void draw_grid_3d(ImDrawList* draw_list, const EditorSceneRect& scene_rect) const;
        void draw_light_direction_3d(const Editor& editor,
                                     ImDrawList* draw_list,
                                     const EditorSceneRect& scene_rect) const;
        void draw_camera_frustum_3d(const Editor& editor,
                                    ImDrawList* draw_list,
                                    const EditorSceneRect& scene_rect) const;
        void draw_selection_overlay_3d(const Editor& editor,
                                       ImDrawList* draw_list,
                                       const EditorSceneRect& scene_rect) const;
        void draw_camera_view_rect(const Editor& editor,
                                   ImDrawList* draw_list,
                                   const EditorSceneRect& scene_rect) const;
        void draw_selection_overlay(const Editor& editor,
                                    ImDrawList* draw_list,
                                    const EditorSceneRect& scene_rect) const;
    };
} // namespace hob::editor
