#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_instances.h"
#include "editor/editor_style.h"
#include "editor_dock_scene_view.h"
#include "engine/components/camera_component_3d.h"
#include "engine/components/directional_light_component.h"
#include "engine/components/mesh_renderer_component.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/constants.h"
#include "engine/math/plane.h"

namespace hob::editor {
    namespace {
        constexpr float PICK_FALLBACK_RADIUS_METERS = 0.5f;
        constexpr float PICK_CYCLE_TOLERANCE_PX = 5.0f;
        constexpr float DROP_FALLBACK_DISTANCE_METERS = 10.0f;
        constexpr int32_t GRID_HALF_EXTENT_CELLS = 50;
        constexpr float GRID_CELL_METERS = 1.0f;
        constexpr int32_t GRID_MAJOR_EVERY = 5;
        constexpr float FRUSTUM_DISPLAY_FAR_METERS = 50.0f;
        constexpr float LIGHT_GIZMO_LENGTH_METERS = 3.0f;
        constexpr float LIGHT_GIZMO_RADIUS_METERS = 0.35f;
        constexpr float SELECTION_MARKER_RADIUS_PX = 14.0f;

        constexpr std::array<std::pair<int32_t, int32_t>, 12> BOX_EDGES = {
            {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};

        bool ray_hits_sphere(const Ray& ray, const Vector3& center, float radius, float& out_distance) {
            const Vector3 to_center = center - ray.origin;
            const float projected = Vector3::dot(to_center, ray.direction);
            const float perpendicular_sqr = to_center.length_sqr() - projected * projected;
            const float radius_sqr = radius * radius;
            if (perpendicular_sqr > radius_sqr) {
                return false;
            }

            const float half_chord = std::sqrt(radius_sqr - perpendicular_sqr);
            out_distance = projected - half_chord;
            if (out_distance < 0.0f) {
                out_distance = projected + half_chord;
            }

            return out_distance >= 0.0f;
        }

        bool ray_hits_mesh_bounds(const Ray& ray, const MeshRendererComponent& mesh_renderer, float& out_distance) {
            const Matrix4x4 to_local = mesh_renderer.get_entity().get_transform_3d()->get_world_matrix().inverse();
            const Vector3 local_origin = to_local.transform_point(ray.origin);
            const Vector3 local_direction = to_local.transform_direction(ray.direction);

            return mesh_renderer.get_local_bounds().intersect_ray(local_origin, local_direction, out_distance);
        }

        AABB3 compute_entity_world_bounds_3d(const Entity& entity) {
            if (const MeshRendererComponent* mesh_renderer = entity.get_component<MeshRendererComponent>()) {
                return mesh_renderer->get_world_bounds();
            }

            return AABB3(entity.get_transform_3d()->get_position(), Vector3::one() * PICK_FALLBACK_RADIUS_METERS);
        }

        void draw_world_box(ImDrawList* draw_list,
                            const EditorCamera3D& camera,
                            const EditorSceneRect& scene_rect,
                            const std::array<Vector3, 8>& corners,
                            ImU32 color,
                            float thickness) {
            for (const auto& [a, b] : BOX_EDGES) {
                Vector2 screen_a;
                Vector2 screen_b;
                if (camera.project_segment(corners[a], corners[b], scene_rect, screen_a, screen_b)) {
                    draw_list->AddLine(to_imvec(screen_a), to_imvec(screen_b), color, thickness);
                }
            }
        }
    } // namespace

    bool EditorDockSceneView::is_flying() const {
        return m_flying;
    }

    void EditorDockSceneView::update_input_3d(Editor& editor) {
        const ImGuiIO& io = ImGui::GetIO();
        const Vector2 mouse_screen_pos(io.MousePos.x, io.MousePos.y);

        if (m_flying && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            m_flying = false;
        }

        if (!m_flying && (!m_rect_valid || !m_hovered)) {
            return;
        }

        if (!m_flying && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_flying = true;
        }

        if (m_flying) {
            m_camera_3d.look_by_pixel_delta(Vector2(io.MouseDelta.x, io.MouseDelta.y));

            Vector3 direction;
            direction.z += ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f;
            direction.z -= ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f;
            direction.x += ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f;
            direction.x -= ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f;
            direction.y += ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f;
            direction.y -= ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f;
            m_camera_3d.fly(direction, io.DeltaTime, io.KeyShift);

            if (io.MouseWheel != 0.0f) {
                m_camera_3d.adjust_speed(io.MouseWheel);
            }

            return;
        }

        if (io.MouseWheel != 0.0f) {
            m_camera_3d.dolly(io.MouseWheel);
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            m_camera_3d.pan_by_pixel_delta(Vector2(io.MouseDelta.x, io.MouseDelta.y), m_rect);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_gizmo_3d.is_hovered() && !m_gizmo_3d.is_using()) {
            handle_pick_3d(editor, mouse_screen_pos);
        }
    }

    void EditorDockSceneView::draw_3d(Editor& editor, ImDrawList* draw_list, const EditorSceneRect& scene_rect) {
        handle_prefab_drop_3d(editor, scene_rect);
        draw_grid_3d(draw_list, scene_rect);
        draw_camera_frustum_3d(editor, draw_list, scene_rect);
        draw_light_direction_3d(editor, draw_list, scene_rect);
        draw_selection_overlay_3d(editor, draw_list, scene_rect);
        m_gizmo_3d.update_and_draw(editor, draw_list, m_camera_3d, scene_rect);
    }

    void EditorDockSceneView::render_pass_3d(Editor& editor) {
        if (m_color_target == nullptr || !m_targets_3d.is_valid()) {
            return;
        }

        const Vector2 scene_size(static_cast<float>(m_color_target_width), static_cast<float>(m_color_target_height));
        editor.get_engine().get_renderer().render_world_pass_3d_to(
            m_targets_3d, m_color_target, m_camera_3d.build_view_projection(scene_size), m_camera_3d.position);
    }

    void EditorDockSceneView::focus_on_selection_3d(const Editor& editor) {
        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();

        std::optional<AABB3> world_bounds;
        for (EntityId id : editor.get_selection().ids) {
            const Entity* entity = spawner.get_entity(id);
            if (entity == nullptr || entity->get_transform_3d() == nullptr) {
                continue;
            }

            const AABB3 entity_bounds = compute_entity_world_bounds_3d(*entity);
            world_bounds = world_bounds.has_value() ? AABB3::combine(*world_bounds, entity_bounds) : entity_bounds;
        }

        if (world_bounds.has_value()) {
            m_camera_3d.focus_on(*world_bounds);
        }
    }

    void EditorDockSceneView::handle_pick_3d(Editor& editor, const Vector2& mouse_screen_pos) {
        const bool additive = ImGui::GetIO().KeyCtrl;
        EditorSelection& selection = editor.get_selection();

        std::vector<EntityId> candidates;
        gather_pick_candidates_3d(editor, m_camera_3d.screen_to_ray(mouse_screen_pos, m_rect), candidates);

        if (candidates.empty()) {
            if (!additive) {
                selection.clear();
            }

            m_pick_cycle_last_entity_id = INVALID_ENTITY_ID;
            return;
        }

        size_t index = 0;
        const Vector2 cycle_screen_delta = mouse_screen_pos - m_pick_cycle_screen_position;
        if (cycle_screen_delta.length() <= PICK_CYCLE_TOLERANCE_PX) {
            const auto it = std::find(candidates.begin(), candidates.end(), m_pick_cycle_last_entity_id);
            if (it != candidates.end()) {
                index = (static_cast<size_t>(it - candidates.begin()) + 1) % candidates.size();
            }
        }

        const EntityId picked_entity_id = candidates[index];
        m_pick_cycle_screen_position = mouse_screen_pos;
        m_pick_cycle_last_entity_id = picked_entity_id;

        selection.apply_click(EditorSelectionClick{picked_entity_id, additive, false}, {});
        editor.get_hierarchy().scroll_to_primary();
    }

    void EditorDockSceneView::gather_pick_candidates_3d(const Editor& editor,
                                                        const Ray& ray,
                                                        std::vector<EntityId>& out_candidates) const {
        out_candidates.clear();

        struct EditorRayHit {
            EntityId entity_id;
            float distance;
        };

        std::vector<EditorRayHit> hits;

        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();
        for (const MeshRendererComponent* mesh_renderer : spawner.get_mesh_renderers()) {
            float distance = 0.0f;
            if (ray_hits_mesh_bounds(ray, *mesh_renderer, distance)) {
                hits.emplace_back(mesh_renderer->get_entity().get_id(), distance);
            }
        }

        if (hits.empty()) {
            std::vector<Entity*> entities;
            spawner.get_entities(entities);
            for (const Entity* entity : entities) {
                const TransformComponent3D* transform = entity->get_transform_3d();
                if (transform == nullptr) {
                    continue;
                }

                float distance = 0.0f;
                if (ray_hits_sphere(ray, transform->get_position(), PICK_FALLBACK_RADIUS_METERS, distance)) {
                    hits.emplace_back(entity->get_id(), distance);
                }
            }
        }

        std::sort(hits.begin(), hits.end(), [](const EditorRayHit& a, const EditorRayHit& b) {
            return a.distance < b.distance;
        });

        for (const EditorRayHit& hit : hits) {
            out_candidates.push_back(hit.entity_id);
        }
    }

    void EditorDockSceneView::handle_prefab_drop_3d(Editor& editor, const EditorSceneRect& scene_rect) {
        if (!ImGui::BeginDragDropTarget()) {
            return;
        }

        const std::optional<std::string> prefab_name = accept_drag_payload(DRAG_PAYLOAD_PREFAB);
        if (prefab_name.has_value()) {
            const ImVec2 mouse_pos = ImGui::GetMousePos();
            const Ray ray = m_camera_3d.screen_to_ray(Vector2(mouse_pos.x, mouse_pos.y), scene_rect);

            float distance = DROP_FALLBACK_DISTANCE_METERS;
            Plane::from_point_normal(Vector3::zero(), Vector3::up()).raycast(ray, distance);
            add_prefab_instance(editor, *prefab_name, ray.point_at(distance));
        }

        ImGui::EndDragDropTarget();
    }

    void EditorDockSceneView::draw_grid_3d(ImDrawList* draw_list, const EditorSceneRect& scene_rect) const {
        const ImU32 minor_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_MINOR);
        const ImU32 major_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_MAJOR);
        const ImU32 axis_x_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_AXIS_X);
        const ImU32 axis_z_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_AXIS_Z);

        const float center_x = std::round(m_camera_3d.position.x / GRID_CELL_METERS) * GRID_CELL_METERS;
        const float center_z = std::round(m_camera_3d.position.z / GRID_CELL_METERS) * GRID_CELL_METERS;
        const float half_extent = GRID_HALF_EXTENT_CELLS * GRID_CELL_METERS;

        const auto draw_world_line = [&](const Vector3& a, const Vector3& b, ImU32 color) {
            Vector2 screen_a;
            Vector2 screen_b;
            if (m_camera_3d.project_segment(a, b, scene_rect, screen_a, screen_b)) {
                draw_list->AddLine(to_imvec(screen_a), to_imvec(screen_b), color);
            }
        };

        for (int32_t i = -GRID_HALF_EXTENT_CELLS; i <= GRID_HALF_EXTENT_CELLS; ++i) {
            const float x = center_x + static_cast<float>(i) * GRID_CELL_METERS;
            const float z = center_z + static_cast<float>(i) * GRID_CELL_METERS;

            if (std::abs(x) >= GRID_CELL_METERS * 0.5f) {
                const bool major = std::llround(x / GRID_CELL_METERS) % GRID_MAJOR_EVERY == 0;
                draw_world_line(Vector3(x, 0.0f, center_z - half_extent),
                                Vector3(x, 0.0f, center_z + half_extent),
                                major ? major_color : minor_color);
            }

            if (std::abs(z) >= GRID_CELL_METERS * 0.5f) {
                const bool major = std::llround(z / GRID_CELL_METERS) % GRID_MAJOR_EVERY == 0;
                draw_world_line(Vector3(center_x - half_extent, 0.0f, z),
                                Vector3(center_x + half_extent, 0.0f, z),
                                major ? major_color : minor_color);
            }
        }

        draw_world_line(
            Vector3(center_x - half_extent, 0.0f, 0.0f), Vector3(center_x + half_extent, 0.0f, 0.0f), axis_x_color);
        draw_world_line(
            Vector3(0.0f, 0.0f, center_z - half_extent), Vector3(0.0f, 0.0f, center_z + half_extent), axis_z_color);
    }

    void EditorDockSceneView::draw_light_direction_3d(const Editor& editor,
                                                      ImDrawList* draw_list,
                                                      const EditorSceneRect& scene_rect) const {
        const DirectionalLightComponent* light = editor.get_engine().get_active_directional_light();
        if (light == nullptr) {
            return;
        }

        const Vector3 origin = light->get_entity().get_transform_3d()->get_position();
        const Vector3 direction = light->get_direction();
        const ImU32 color = ImGui::GetColorU32(COLOR_SCENE_VIEW_LIGHT);
        const Vector3 side = Vector3::cross(Vector3::up(), direction).normalized() * LIGHT_GIZMO_RADIUS_METERS;
        const Vector3 side_up = Vector3::cross(direction, side).normalized() * LIGHT_GIZMO_RADIUS_METERS;

        for (const Vector3& offset : {Vector3::zero(), side, -side, side_up, -side_up}) {
            Vector2 screen_a;
            Vector2 screen_b;
            const Vector3 start = origin + offset;
            if (m_camera_3d.project_segment(
                    start, start + direction * LIGHT_GIZMO_LENGTH_METERS, scene_rect, screen_a, screen_b)) {
                draw_list->AddLine(to_imvec(screen_a), to_imvec(screen_b), color, SCENE_VIEW_CAMERA_RECT_THICKNESS);
            }
        }
    }

    void EditorDockSceneView::draw_camera_frustum_3d(const Editor& editor,
                                                     ImDrawList* draw_list,
                                                     const EditorSceneRect& scene_rect) const {
        Engine& engine = editor.get_engine();
        const CameraComponent3D* camera = engine.get_active_camera_3d();
        if (camera == nullptr) {
            return;
        }

        const Renderer& renderer = engine.get_renderer();
        const Vector2 view_size =
            (engine.get_game_window() != nullptr) ? renderer.get_logical_size() : renderer.get_reference_size();
        const Matrix4x4 to_world = camera->build_display_view_projection(view_size).inverse();

        std::array<Vector3, 8> corners;
        for (int32_t i = 0; i < 4; ++i) {
            const float ndc_x = (i & 1) ? 1.0f : -1.0f;
            const float ndc_y = (i & 2) ? 1.0f : -1.0f;

            Vector3 near_corner;
            Vector3 far_corner;
            if (!to_world.project_point(Vector3(ndc_x, ndc_y, 0.0f), near_corner) ||
                !to_world.project_point(Vector3(ndc_x, ndc_y, 1.0f), far_corner)) {
                return;
            }

            const Vector3 direction = far_corner - near_corner;
            const float length = std::min(direction.length(), FRUSTUM_DISPLAY_FAR_METERS);
            corners[i] = near_corner;
            corners[i + 4] = near_corner + direction.normalized() * length;
        }

        draw_world_box(draw_list,
                       m_camera_3d,
                       scene_rect,
                       corners,
                       ImGui::GetColorU32(COLOR_SCENE_VIEW_CAMERA_RECT),
                       SCENE_VIEW_CAMERA_RECT_THICKNESS);
    }

    void EditorDockSceneView::draw_selection_overlay_3d(const Editor& editor,
                                                        ImDrawList* draw_list,
                                                        const EditorSceneRect& scene_rect) const {
        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();
        const EditorSelection& selection = editor.get_selection();
        const EntityId primary_entity_id = selection.primary();

        for (EntityId id : selection.ids) {
            const Entity* entity = spawner.get_entity(id);
            if (entity == nullptr || entity->get_transform_3d() == nullptr) {
                continue;
            }

            const ImU32 color = ImGui::GetColorU32((id == primary_entity_id) ? COLOR_SCENE_VIEW_SELECTION_PRIMARY
                                                                             : COLOR_SCENE_VIEW_SELECTION);

            if (const MeshRendererComponent* mesh_renderer = entity->get_component<MeshRendererComponent>()) {
                std::array<Vector3, 8> local_corners;
                mesh_renderer->get_local_bounds().get_corners(local_corners);

                const Matrix4x4& world = entity->get_transform_3d()->get_world_matrix();
                std::array<Vector3, 8> world_corners;
                for (size_t i = 0; i < local_corners.size(); ++i) {
                    world_corners[i] = world.transform_point(local_corners[i]);
                }

                draw_world_box(
                    draw_list, m_camera_3d, scene_rect, world_corners, color, SCENE_VIEW_SELECTION_OUTLINE_THICKNESS);
            }
            else {
                Vector2 screen_pos;
                if (m_camera_3d.world_to_screen(entity->get_transform_3d()->get_position(), scene_rect, screen_pos)) {
                    draw_list->AddCircle(to_imvec(screen_pos),
                                         SELECTION_MARKER_RADIUS_PX,
                                         color,
                                         0,
                                         SCENE_VIEW_SELECTION_OUTLINE_THICKNESS);
                }
            }
        }
    }
} // namespace hob::editor
