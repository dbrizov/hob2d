#include "editor_dock_scene_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include <imgui.h>

#include "editor/actions/editor_action.h"
#include "editor/editor.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_instances.h"
#include "editor/editor_style.h"
#include "engine/components/camera_component.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/math/aabb.h"
#include "engine/math/constants.h"
#include "engine/math/matrix2x3.h"

namespace hob::editor {
    namespace {
        struct EditorSceneViewToolItem {
            EditorActionId id;
            EditorBarIcon icon;
        };

        constexpr EditorSceneViewToolItem SCENE_VIEW_TOOL_ITEMS[] = {
            {EditorActionId::GizmoTranslateRotate, EditorBarIcon::TranslateRotate},
            {EditorActionId::GizmoTranslate, EditorBarIcon::Translate},
            {EditorActionId::GizmoRotate, EditorBarIcon::Rotate},
            {EditorActionId::GizmoScale, EditorBarIcon::Scale},
        };

        constexpr const char* DOCK_LABEL = "Scene";
        constexpr const char* DOCK_LABEL_DIRTY = "Scene (*)";

        constexpr float MIN_SCENE_RECT_SIZE_PX = 8.0f;
        constexpr uint32_t MIN_COLOR_TARGET_SIZE_PX = 1;

        // Hit radius around a sprite-less entity's origin. Independent of the marker drawn for it,
        // which is a touch larger so the outline sits just outside what it selects.
        constexpr float PICK_RADIUS_PX = 12.0f;

        // How far the cursor may move and still count as "clicking the same spot", which is what
        // steps to the next candidate under the previous pick.
        constexpr float PICK_CYCLE_TOLERANCE_PX = 5.0f;

        // Half-extent, in meters, of the box F frames around a sprite-less entity.
        constexpr float FOCUS_FALLBACK_EXTENT = 0.5f;

        constexpr float GRID_CELL_METERS = 1.0f;
        constexpr int32_t GRID_MAJOR_EVERY = 5;
        constexpr float MIN_GRID_SPACING_PX = 24.0f;

        float grid_cell_meters(float pixels_per_meter) {
            float step = GRID_CELL_METERS;
            while (step * pixels_per_meter < MIN_GRID_SPACING_PX) {
                step *= static_cast<float>(GRID_MAJOR_EVERY);
            }

            return step;
        }

        bool is_major_grid_line(float world_value, float cell_meters) {
            const long long index = std::llround(world_value / cell_meters);
            return index % GRID_MAJOR_EVERY == 0;
        }

        struct EditorSpriteRect {
            Matrix2x3 world_matrix = Matrix2x3::identity();
            AABB local_rect;
        };

        bool compute_sprite_rect(const SpriteComponent& sprite, EditorSpriteRect& out_rect) {
            if (sprite.get_texture() == nullptr) {
                return false;
            }

            out_rect.world_matrix = sprite.get_entity().get_transform()->get_world_matrix();
            out_rect.local_rect = sprite.get_local_rect();

            return true;
        }

        bool is_point_inside_sprite_rect(const EditorSpriteRect& rect, const Vector2& world_pos) {
            const Vector2 local_pos = rect.world_matrix.inverse().transform_point(world_pos);
            const Vector2 min = rect.local_rect.min();
            const Vector2 max = rect.local_rect.max();

            return local_pos.x >= min.x && local_pos.x <= max.x && local_pos.y >= min.y && local_pos.y <= max.y;
        }

        void compute_sprite_rect_corners(const EditorSpriteRect& rect, Vector2 (&out_corners)[4]) {
            const Vector2 min = rect.local_rect.min();
            const Vector2 max = rect.local_rect.max();

            out_corners[0] = rect.world_matrix.transform_point(Vector2(min.x, min.y));
            out_corners[1] = rect.world_matrix.transform_point(Vector2(max.x, min.y));
            out_corners[2] = rect.world_matrix.transform_point(Vector2(max.x, max.y));
            out_corners[3] = rect.world_matrix.transform_point(Vector2(min.x, max.y));
        }

        AABB compute_entity_world_bounds(const Entity& entity) {
            const SpriteComponent* sprite = entity.get_component<SpriteComponent>();

            EditorSpriteRect rect;
            if (sprite != nullptr && compute_sprite_rect(*sprite, rect)) {
                Vector2 world_corners[4];
                compute_sprite_rect_corners(rect, world_corners);

                Vector2 world_min = world_corners[0];
                Vector2 world_max = world_corners[0];
                for (int32_t i = 1; i < 4; ++i) {
                    world_min =
                        Vector2(std::min(world_min.x, world_corners[i].x), std::min(world_min.y, world_corners[i].y));
                    world_max =
                        Vector2(std::max(world_max.x, world_corners[i].x), std::max(world_max.y, world_corners[i].y));
                }

                return AABB::from_min_max(world_min, world_max);
            }

            return AABB(entity.get_transform()->get_position(), Vector2(FOCUS_FALLBACK_EXTENT, FOCUS_FALLBACK_EXTENT));
        }
    } // namespace

    EditorDockSceneView::EditorDockSceneView()
        : EditorDock("Scene", EditorActionContext::SceneView) {}

    void EditorDockSceneView::update_input(Editor& editor) {
        const bool dragging = m_gizmo.is_dragging();
        if (!dragging && (!m_rect_valid || !m_hovered)) {
            m_gizmo.clear_hover();
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const Vector2 mouse_screen_pos(io.MousePos.x, io.MousePos.y);

        if (m_gizmo.update_input(editor, mouse_screen_pos, m_camera, m_rect)) {
            return;
        }

        if (io.MouseWheel != 0.0f) {
            m_camera.zoom_at(mouse_screen_pos, m_rect, io.MouseWheel);
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            m_camera.pan_by_pixel_delta(Vector2(io.MouseDelta.x, io.MouseDelta.y));
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            handle_pick(editor, mouse_screen_pos, m_camera.screen_to_world(mouse_screen_pos, m_rect));
        }
    }

    void EditorDockSceneView::draw(Editor& editor) {
        m_rect_valid = false;

        set_label(editor.is_scene_dirty() ? DOCK_LABEL_DIRTY : DOCK_LABEL);

        EditorStyleColorStack colors;
        colors.push(ImGuiCol_MenuBarBg, COLOR_DOCK_TOOLBAR_BG);

        EditorStyleVarStack vars;
        vars.push(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        const bool visible = begin(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);

        vars.pop();
        colors.pop();

        if (visible) {
            // The image fills the dock, so hovering it is what counts -- not the tab bar above it.
            m_hovered = false;

            draw_toolbar(editor);

            const ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x > MIN_SCENE_RECT_SIZE_PX && avail.y > MIN_SCENE_RECT_SIZE_PX) {
                ensure_color_target(editor, static_cast<uint32_t>(avail.x), static_cast<uint32_t>(avail.y));

                if (m_color_target != nullptr) {
                    const Vector2 image_size(static_cast<float>(m_color_target_width),
                                             static_cast<float>(m_color_target_height));

                    ImGui::Image(
                        m_color_target, ImVec2(image_size.x, image_size.y), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

                    const ImVec2 item_min = ImGui::GetItemRectMin();
                    const EditorSceneRect scene_rect{Vector2(item_min.x, item_min.y), image_size};

                    m_rect = scene_rect;
                    m_rect_valid = true;
                    m_hovered = ImGui::IsItemHovered();

                    handle_prefab_drop(editor, scene_rect);

                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    draw_list->PushClipRect(
                        item_min, ImVec2(item_min.x + image_size.x, item_min.y + image_size.y), true);
                    draw_grid(draw_list, scene_rect);
                    draw_camera_view_rect(editor, draw_list, scene_rect);
                    draw_selection_overlay(editor, draw_list, scene_rect);
                    m_gizmo.draw(editor, draw_list, m_camera, scene_rect);
                    draw_list->PopClipRect();
                }
            }
        }
        end();
    }

    void EditorDockSceneView::render_pass(Editor& editor) {
        if (m_color_target == nullptr) {
            return;
        }

        const Vector2 scene_size(static_cast<float>(m_color_target_width), static_cast<float>(m_color_target_height));
        const Matrix4x4 view_proj = m_camera.build_view_projection(scene_size);

        editor.get_engine().get_renderer().render_world_pass_to(m_color_target, view_proj);
    }

    void EditorDockSceneView::release_color_target(Editor& editor) {
        if (m_color_target == nullptr) {
            return;
        }

        SDL_ReleaseGPUTexture(editor.get_engine().get_renderer().get_gpu_device(), m_color_target);
        m_color_target = nullptr;
        m_color_target_width = 0;
        m_color_target_height = 0;
    }

    EditorGizmoMode EditorDockSceneView::get_gizmo_mode() const {
        return m_gizmo.get_mode();
    }

    void EditorDockSceneView::set_gizmo_mode(EditorGizmoMode mode) {
        m_gizmo.set_mode(mode);
    }

    EditorGizmoSpace EditorDockSceneView::get_gizmo_space() const {
        return m_gizmo.get_space();
    }

    void EditorDockSceneView::toggle_gizmo_space() {
        m_gizmo.toggle_space();
    }

    void EditorDockSceneView::reset_gizmo() {
        m_gizmo.reset();
    }

    void EditorDockSceneView::focus_on_selection(const Editor& editor) {
        if (!m_rect_valid) {
            return;
        }

        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();

        std::optional<AABB> world_bounds;
        for (EntityId id : editor.get_selection().ids) {
            const Entity* entity = spawner.get_entity(id);
            if (entity == nullptr) {
                continue;
            }

            const AABB entity_bounds = compute_entity_world_bounds(*entity);
            world_bounds = world_bounds.has_value() ? AABB::combine(*world_bounds, entity_bounds) : entity_bounds;
        }

        if (world_bounds.has_value()) {
            m_camera.focus_on(*world_bounds, m_rect);
        }
    }

    void EditorDockSceneView::reset_pick_cycle() {
        m_pick_cycle_last_entity_id = INVALID_ENTITY_ID;
    }

    void EditorDockSceneView::ensure_color_target(Editor& editor, uint32_t width, uint32_t height) {
        width = std::max(width, MIN_COLOR_TARGET_SIZE_PX);
        height = std::max(height, MIN_COLOR_TARGET_SIZE_PX);

        if (m_color_target != nullptr && width == m_color_target_width && height == m_color_target_height) {
            return;
        }

        release_color_target(editor);

        m_color_target = editor.get_engine().get_renderer().create_color_target(width, height);
        if (m_color_target == nullptr) {
            return;
        }

        m_color_target_width = width;
        m_color_target_height = height;
    }

    void EditorDockSceneView::handle_pick(Editor& editor,
                                          const Vector2& mouse_screen_pos,
                                          const Vector2& mouse_world_pos) {
        const bool additive = ImGui::GetIO().KeyCtrl;
        EditorSelection& selection = editor.get_selection();

        std::vector<EntityId> candidates;
        gather_pick_candidates(editor, mouse_world_pos, candidates);

        if (candidates.empty()) {
            if (!additive) {
                selection.clear();
            }

            m_pick_cycle_last_entity_id = INVALID_ENTITY_ID;
            return;
        }

        // Clicking the same spot again steps to the next candidate underneath the previous one.
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

    void EditorDockSceneView::gather_pick_candidates(const Editor& editor,
                                                     const Vector2& world_pos,
                                                     std::vector<EntityId>& out_candidates) const {
        out_candidates.clear();

        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();
        const std::vector<SpriteComponent*>& sprites = spawner.get_sprites();

        struct EditorSpriteHit {
            EntityId entity_id;
            int32_t z_index;
            int32_t sprite_index;
        };

        std::vector<EditorSpriteHit> sprite_hits;
        for (int32_t i = 0; i < sprites.size(); ++i) {
            const SpriteComponent* sprite = sprites[i];

            EditorSpriteRect rect;
            if (!compute_sprite_rect(*sprite, rect) || !is_point_inside_sprite_rect(rect, world_pos)) {
                continue;
            }

            sprite_hits.emplace_back(sprite->get_entity().get_id(), sprite->get_z_index(), i);
        }

        if (!sprite_hits.empty()) {
            // Topmost first: higher z wins, and at equal z the later-registered sprite draws on top.
            std::sort(sprite_hits.begin(), sprite_hits.end(), [](const EditorSpriteHit& a, const EditorSpriteHit& b) {
                return (a.z_index != b.z_index) ? (a.z_index > b.z_index) : (a.sprite_index > b.sprite_index);
            });

            for (const EditorSpriteHit& hit : sprite_hits) {
                out_candidates.push_back(hit.entity_id);
            }

            return;
        }

        // Sprite-less entities:
        // a fixed screen radius around the origin, so the grab area stays the same size on screen at any zoom.
        const float pick_radius_world = PICK_RADIUS_PX / m_camera.get_pixels_per_meter_f();

        struct EditorOriginHit {
            EntityId entity_id;
            float world_distance;
        };

        std::vector<Entity*> entities;
        spawner.get_entities(entities);

        std::vector<EditorOriginHit> origin_hits;
        for (const Entity* entity : entities) {
            const float world_distance = (entity->get_transform()->get_position() - world_pos).length();
            if (world_distance <= pick_radius_world) {
                origin_hits.emplace_back(entity->get_id(), world_distance);
            }
        }

        std::sort(origin_hits.begin(), origin_hits.end(), [](const EditorOriginHit& a, const EditorOriginHit& b) {
            return a.world_distance < b.world_distance;
        });

        for (const EditorOriginHit& hit : origin_hits) {
            out_candidates.push_back(hit.entity_id);
        }
    }

    void EditorDockSceneView::handle_prefab_drop(Editor& editor, const EditorSceneRect& scene_rect) {
        if (!ImGui::BeginDragDropTarget()) {
            return;
        }

        const std::optional<std::string> prefab_name = accept_drag_payload(DRAG_PAYLOAD_PREFAB);
        if (prefab_name.has_value()) {
            const ImVec2 mouse_pos = ImGui::GetMousePos();
            add_prefab_instance(
                editor, *prefab_name, m_camera.screen_to_world(Vector2(mouse_pos.x, mouse_pos.y), scene_rect));
        }

        ImGui::EndDragDropTarget();
    }

    void EditorDockSceneView::draw_toolbar(Editor& editor) {
        if (!ImGui::BeginMenuBar()) {
            return;
        }

        for (const EditorSceneViewToolItem& item : SCENE_VIEW_TOOL_ITEMS) {
            action_bar_icon_button(editor, item.id, item.icon);
        }

        const EditorBarIcon space_icon =
            (m_gizmo.get_space() == EditorGizmoSpace::Local) ? EditorBarIcon::SpaceLocal : EditorBarIcon::SpaceWorld;
        action_bar_icon_button(editor, EditorActionId::GizmoToggleSpace, space_icon);

        ImGui::EndMenuBar();
    }

    void EditorDockSceneView::draw_grid(ImDrawList* draw_list, const EditorSceneRect& scene_rect) const {
        const float cell_meters = grid_cell_meters(m_camera.get_pixels_per_meter_f());

        const Vector2 top_left = scene_rect.top_left;
        const Vector2 bottom_right = scene_rect.top_left + scene_rect.size;

        // Y is inverted, so the world minimum is at the image's bottom-left corner.
        const Vector2 world_min = m_camera.screen_to_world(Vector2(top_left.x, bottom_right.y), scene_rect);
        const Vector2 world_max = m_camera.screen_to_world(Vector2(bottom_right.x, top_left.y), scene_rect);

        const ImU32 minor_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_MINOR);
        const ImU32 major_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_MAJOR);
        const ImU32 axis_x_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_AXIS_X);
        const ImU32 axis_y_color = ImGui::GetColorU32(COLOR_SCENE_VIEW_GRID_AXIS_Y);

        // Vertical lines: the one at world x == 0 is the Y axis, drawn separately below.
        const float world_start_x = std::floor(world_min.x / cell_meters) * cell_meters;
        for (float world_x = world_start_x; world_x <= world_max.x; world_x += cell_meters) {
            if (std::abs(world_x) < cell_meters * 0.5f) {
                continue;
            }

            const float screen_x = m_camera.world_to_screen(Vector2(world_x, 0.0f), scene_rect).x;
            const ImU32 color = is_major_grid_line(world_x, cell_meters) ? major_color : minor_color;
            draw_list->AddLine(ImVec2(screen_x, top_left.y), ImVec2(screen_x, bottom_right.y), color);
        }

        // Horizontal lines: the one at world y == 0 is the X axis, drawn separately below.
        const float world_start_y = std::floor(world_min.y / cell_meters) * cell_meters;
        for (float world_y = world_start_y; world_y <= world_max.y; world_y += cell_meters) {
            if (std::abs(world_y) < cell_meters * 0.5f) {
                continue;
            }

            const float screen_y = m_camera.world_to_screen(Vector2(0.0f, world_y), scene_rect).y;
            const ImU32 color = is_major_grid_line(world_y, cell_meters) ? major_color : minor_color;
            draw_list->AddLine(ImVec2(top_left.x, screen_y), ImVec2(bottom_right.x, screen_y), color);
        }

        // The axes are the origin lines, always drawn regardless of how dense the grid got.
        if (world_min.x <= 0.0f && world_max.x >= 0.0f) {
            const float screen_x = m_camera.world_to_screen(Vector2(0.0f, 0.0f), scene_rect).x;
            draw_list->AddLine(ImVec2(screen_x, top_left.y), ImVec2(screen_x, bottom_right.y), axis_y_color);
        }

        if (world_min.y <= 0.0f && world_max.y >= 0.0f) {
            const float screen_y = m_camera.world_to_screen(Vector2(0.0f, 0.0f), scene_rect).y;
            draw_list->AddLine(ImVec2(top_left.x, screen_y), ImVec2(bottom_right.x, screen_y), axis_x_color);
        }
    }

    void EditorDockSceneView::draw_camera_view_rect(const Editor& editor,
                                                    ImDrawList* draw_list,
                                                    const EditorSceneRect& scene_rect) const {
        Engine& engine = editor.get_engine();

        const CameraComponent* camera = engine.get_active_camera();
        if (camera == nullptr) {
            return;
        }

        const Renderer& renderer = engine.get_renderer();
        const Vector2 view_size_px =
            (engine.get_game_window() != nullptr) ? renderer.get_logical_size() : renderer.get_reference_size();

        const float camera_ppm = camera->get_effective_pixels_per_meter();
        if (view_size_px.x <= EPSILON || view_size_px.y <= EPSILON || camera_ppm <= EPSILON) {
            return;
        }

        const AABB view_bounds(camera->get_entity().get_transform()->get_position(), view_size_px * 0.5f / camera_ppm);

        // World +Y is up but the screen's +Y is down, so the world top-left corner is (min.x, max.y).
        const Vector2 top_left =
            m_camera.world_to_screen(Vector2(view_bounds.min().x, view_bounds.max().y), scene_rect);
        const Vector2 bottom_right =
            m_camera.world_to_screen(Vector2(view_bounds.max().x, view_bounds.min().y), scene_rect);

        draw_list->AddRect(to_imvec(top_left),
                           to_imvec(bottom_right),
                           ImGui::GetColorU32(COLOR_SCENE_VIEW_CAMERA_RECT),
                           0.0f,
                           ImDrawFlags_None,
                           SCENE_VIEW_CAMERA_RECT_THICKNESS);
    }

    void EditorDockSceneView::draw_selection_overlay(const Editor& editor,
                                                     ImDrawList* draw_list,
                                                     const EditorSceneRect& scene_rect) const {
        const EntitySpawner& spawner = editor.get_engine().get_entity_spawner();
        const EditorSelection& selection = editor.get_selection();
        const EntityId primary_entity_id = selection.primary();

        // selection.ids is ordered with the primary last, so it naturally paints on top.
        for (EntityId id : selection.ids) {
            const Entity* entity = spawner.get_entity(id);
            if (entity == nullptr) {
                continue;
            }

            const ImU32 color = ImGui::GetColorU32((id == primary_entity_id) ? COLOR_SCENE_VIEW_SELECTION_PRIMARY
                                                                             : COLOR_SCENE_VIEW_SELECTION);

            const SpriteComponent* sprite = entity->get_component<SpriteComponent>();

            EditorSpriteRect rect;
            if (sprite != nullptr && compute_sprite_rect(*sprite, rect)) {
                Vector2 world_corners[4];
                compute_sprite_rect_corners(rect, world_corners);

                const ImVec2 screen_corners[4] = {
                    to_imvec(m_camera.world_to_screen(world_corners[0], scene_rect)),
                    to_imvec(m_camera.world_to_screen(world_corners[1], scene_rect)),
                    to_imvec(m_camera.world_to_screen(world_corners[2], scene_rect)),
                    to_imvec(m_camera.world_to_screen(world_corners[3], scene_rect)),
                };

                draw_list->AddPolyline(
                    screen_corners, 4, color, ImDrawFlags_Closed, SCENE_VIEW_SELECTION_OUTLINE_THICKNESS);
            }
            else {
                const Vector2 world_pos = entity->get_transform()->get_position();

                draw_list->AddCircle(to_imvec(m_camera.world_to_screen(world_pos, scene_rect)),
                                     SCENE_VIEW_SELECTION_MARKER_RADIUS_PX,
                                     color,
                                     0,
                                     SCENE_VIEW_SELECTION_OUTLINE_THICKNESS);
            }
        }
    }

} // namespace hob::editor
