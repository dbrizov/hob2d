#pragma once

#include "matrix4x4.h"
#include "quaternion.h"
#include "ray.h"
#include "vector2.h"
#include "vector3.h"

namespace hob::projection {
    struct Viewport {
        Vector2 top_left;
        Vector2 size;
    };

    Vector2 ndc_to_screen(const Vector3& ndc, const Viewport& viewport);

    bool project_point(const Matrix4x4& display_view_proj,
                       const Vector3& world_pos,
                       const Viewport& viewport,
                       Vector2& out_screen_pos);

    // Clips the segment against the camera near plane before projecting, so a segment that crosses behind the
    // camera still yields a valid on-screen piece.
    bool project_segment(const Matrix4x4& view,
                         const Matrix4x4& display_projection,
                         float near_plane,
                         const Vector3& world_a,
                         const Vector3& world_b,
                         const Viewport& viewport,
                         Vector2& out_screen_a,
                         Vector2& out_screen_b);

    Ray screen_to_ray(const Vector3& camera_position,
                      const Quaternion& camera_rotation,
                      float fov_y_rad,
                      float aspect,
                      const Vector2& screen_pos,
                      const Viewport& viewport);
} // namespace hob::projection
