#include "projection.h"

#include <cmath>

#include "constants.h"

namespace hob::projection {
    Vector2 ndc_to_screen(const Vector3& ndc, const Viewport& viewport) {
        return Vector2(viewport.top_left.x + (ndc.x * 0.5f + 0.5f) * viewport.size.x,
                       viewport.top_left.y + (0.5f - ndc.y * 0.5f) * viewport.size.y);
    }

    bool project_point(const Matrix4x4& display_view_proj,
                       const Vector3& world_pos,
                       const Viewport& viewport,
                       Vector2& out_screen_pos) {
        Vector3 ndc;
        if (!display_view_proj.project_point(world_pos, ndc)) {
            return false;
        }

        out_screen_pos = ndc_to_screen(ndc, viewport);
        return true;
    }

    bool project_segment(const Matrix4x4& view,
                         const Matrix4x4& display_projection,
                         float near_plane,
                         const Vector3& world_a,
                         const Vector3& world_b,
                         const Viewport& viewport,
                         Vector2& out_screen_a,
                         Vector2& out_screen_b) {
        Vector3 a = view.transform_point(world_a);
        Vector3 b = view.transform_point(world_b);

        const bool a_behind = a.z < near_plane;
        const bool b_behind = b.z < near_plane;
        if (a_behind && b_behind) {
            return false;
        }

        if (a_behind || b_behind) {
            const float t = (near_plane - a.z) / (b.z - a.z);
            const Vector3 clipped = Vector3::lerp(a, b, t);
            if (a_behind) {
                a = clipped;
            }
            else {
                b = clipped;
            }
        }

        Vector3 ndc_a;
        Vector3 ndc_b;
        if (!display_projection.project_point(a, ndc_a) || !display_projection.project_point(b, ndc_b)) {
            return false;
        }

        out_screen_a = ndc_to_screen(ndc_a, viewport);
        out_screen_b = ndc_to_screen(ndc_b, viewport);
        return true;
    }

    Ray screen_to_ray(const Vector3& camera_position,
                      const Quaternion& camera_rotation,
                      float fov_y_rad,
                      float aspect,
                      const Vector2& screen_pos,
                      const Viewport& viewport) {
        const float ndc_x =
            viewport.size.x > 0.0f ? ((screen_pos.x - viewport.top_left.x) / viewport.size.x) * 2.0f - 1.0f : 0.0f;
        const float ndc_y =
            viewport.size.y > 0.0f ? 1.0f - ((screen_pos.y - viewport.top_left.y) / viewport.size.y) * 2.0f : 0.0f;

        const float half_fov_tan = std::tan(fov_y_rad * 0.5f);
        const Vector3 view_direction(ndc_x * half_fov_tan * aspect, ndc_y * half_fov_tan, 1.0f);

        return Ray(camera_position, camera_rotation.rotate(view_direction));
    }
} // namespace hob::projection
