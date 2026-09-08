#include "aabb3.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "constants.h"

namespace hob {
    std::string AABB3::to_string() const {
        return std::format("AABB3(center={}, extents={})", center.to_string(), extents.to_string());
    }

    void AABB3::get_corners(std::array<Vector3, 8>& out_corners) const {
        const Vector3 lo = min();
        const Vector3 hi = max();
        out_corners = {Vector3(lo.x, lo.y, lo.z),
                       Vector3(hi.x, lo.y, lo.z),
                       Vector3(lo.x, hi.y, lo.z),
                       Vector3(hi.x, hi.y, lo.z),
                       Vector3(lo.x, lo.y, hi.z),
                       Vector3(hi.x, lo.y, hi.z),
                       Vector3(lo.x, hi.y, hi.z),
                       Vector3(hi.x, hi.y, hi.z)};
    }

    AABB3 AABB3::transformed(const Matrix4x4& matrix) const {
        std::array<Vector3, 8> corners;
        get_corners(corners);

        Vector3 lo = matrix.transform_point(corners[0]);
        Vector3 hi = lo;
        for (size_t i = 1; i < corners.size(); ++i) {
            const Vector3 world = matrix.transform_point(corners[i]);
            lo = Vector3::min(lo, world);
            hi = Vector3::max(hi, world);
        }

        return from_min_max(lo, hi);
    }

    bool AABB3::contains(const Vector3& point) const {
        const Vector3 delta = Vector3::abs(point - center);
        return delta.x <= extents.x && delta.y <= extents.y && delta.z <= extents.z;
    }

    bool AABB3::raycast(const Ray& ray, float& out_distance) const {
        return intersect_ray(ray.origin, ray.direction, out_distance);
    }

    bool AABB3::intersect_ray(const Vector3& ray_origin, const Vector3& ray_direction, float& out_distance) const {
        const Vector3 lo = min();
        const Vector3 hi = max();
        float t_min = 0.0f;
        float t_max = MAX_FLOAT;

        const float origin[3] = {ray_origin.x, ray_origin.y, ray_origin.z};
        const float direction[3] = {ray_direction.x, ray_direction.y, ray_direction.z};
        const float lo_axis[3] = {lo.x, lo.y, lo.z};
        const float hi_axis[3] = {hi.x, hi.y, hi.z};

        for (int32_t axis = 0; axis < 3; ++axis) {
            if (std::abs(direction[axis]) <= EPSILON) {
                if (origin[axis] < lo_axis[axis] || origin[axis] > hi_axis[axis]) {
                    return false;
                }
                continue;
            }

            const float inv_dir = 1.0f / direction[axis];
            float t_near = (lo_axis[axis] - origin[axis]) * inv_dir;
            float t_far = (hi_axis[axis] - origin[axis]) * inv_dir;
            if (t_near > t_far) {
                std::swap(t_near, t_far);
            }

            t_min = std::max(t_min, t_near);
            t_max = std::min(t_max, t_far);
            if (t_min > t_max) {
                return false;
            }
        }

        out_distance = t_min;
        return true;
    }
} // namespace hob
