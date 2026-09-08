#include "camera_component_3d.h"

#include <cmath>

#include "engine/core/engine.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"
#include "engine/math/projection.h"
#include "transform_component_3d.h"

namespace hob {
    namespace {
        float aspect_of(const Vector2& size) {
            return size.y > 0.0f ? size.x / size.y : 1.0f;
        }
    } // namespace

    CameraComponent3D::CameraComponent3D(Entity& entity)
        : Component(entity) {}

    void CameraComponent3D::enter_world() {
        get_engine().set_active_camera_3d(this);
    }

    void CameraComponent3D::exit_world() {
        get_engine().clear_active_camera_3d(this);
    }

    std::string CameraComponent3D::to_string() const {
        return "CameraComponent3D";
    }

    float CameraComponent3D::get_fov_deg() const {
        return m_fov_deg;
    }

    void CameraComponent3D::set_fov_deg(float value) {
        m_fov_deg = value;
    }

    float CameraComponent3D::get_near_plane() const {
        return m_near_plane;
    }

    void CameraComponent3D::set_near_plane(float value) {
        m_near_plane = value;
    }

    float CameraComponent3D::get_far_plane() const {
        return m_far_plane;
    }

    void CameraComponent3D::set_far_plane(float value) {
        m_far_plane = value;
    }

    Vector3 CameraComponent3D::get_position() const {
        return get_entity().get_transform_3d()->get_position();
    }

    Matrix4x4 CameraComponent3D::build_view_matrix() const {
        const TransformComponent3D* transform = get_entity().get_transform_3d();
        return Matrix4x4::trs(transform->get_position(), transform->get_rotation(), Vector3::one()).inverse_affine();
    }

    Matrix4x4 CameraComponent3D::build_view_projection(const Vector2& target_size) const {
        return Renderer::perspective_offscreen(
                   m_fov_deg * DEG_TO_RAD, aspect_of(target_size), m_near_plane, m_far_plane) *
               build_view_matrix();
    }

    Matrix4x4 CameraComponent3D::build_display_view_projection(const Vector2& target_size) const {
        return Matrix4x4::perspective_lh(m_fov_deg * DEG_TO_RAD, aspect_of(target_size), m_near_plane, m_far_plane) *
               build_view_matrix();
    }

    Matrix4x4 CameraComponent3D::build_view_projection() const {
        return build_view_projection(get_engine().get_renderer().get_logical_size());
    }

    bool CameraComponent3D::world_to_screen(const Vector3& world_pos, Vector2& out_screen_pos) const {
        const Vector2 logical_size = get_engine().get_renderer().get_logical_size();
        return projection::project_point(build_display_view_projection(logical_size),
                                         world_pos,
                                         projection::Viewport{Vector2(), logical_size},
                                         out_screen_pos);
    }

    bool CameraComponent3D::project_segment(const Vector3& world_a,
                                            const Vector3& world_b,
                                            Vector2& out_a,
                                            Vector2& out_b) const {
        const Vector2 logical_size = get_engine().get_renderer().get_logical_size();
        return projection::project_segment(
            build_view_matrix(),
            Matrix4x4::perspective_lh(m_fov_deg * DEG_TO_RAD, aspect_of(logical_size), m_near_plane, m_far_plane),
            m_near_plane,
            world_a,
            world_b,
            projection::Viewport{Vector2(), logical_size},
            out_a,
            out_b);
    }

    Ray CameraComponent3D::screen_to_ray(const Vector2& screen_pos) const {
        const Vector2 logical_size = get_engine().get_renderer().get_logical_size();
        const TransformComponent3D* transform = get_entity().get_transform_3d();
        return projection::screen_to_ray(transform->get_position(),
                                         transform->get_rotation(),
                                         m_fov_deg * DEG_TO_RAD,
                                         aspect_of(logical_size),
                                         screen_pos,
                                         projection::Viewport{Vector2(), logical_size});
    }
} // namespace hob
