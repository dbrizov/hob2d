#pragma once

#include <array>
#include <string>

#include "matrix4x4.h"
#include "ray.h"
#include "vector3.h"

namespace hob {
    struct AABB3 {
        Vector3 center;
        Vector3 extents;

        constexpr AABB3() = default;

        constexpr AABB3(const Vector3& center_, const Vector3& extents_)
            : center(center_)
            , extents(extents_) {}

        std::string to_string() const;

        bool operator==(const AABB3& right) const {
            return center == right.center && extents == right.extents;
        }

        bool operator!=(const AABB3& right) const {
            return !operator==(right);
        }

        Vector3 min() const {
            return center - extents;
        }

        Vector3 max() const {
            return center + extents;
        }

        Vector3 size() const {
            return extents * 2.0f;
        }

        static AABB3 from_min_max(const Vector3& min, const Vector3& max) {
            return AABB3((min + max) * 0.5f, (max - min) * 0.5f);
        }

        static AABB3 combine(const AABB3& a, const AABB3& b) {
            return from_min_max(Vector3::min(a.min(), b.min()), Vector3::max(a.max(), b.max()));
        }

        void get_corners(std::array<Vector3, 8>& out_corners) const;
        AABB3 transformed(const Matrix4x4& matrix) const;
        bool contains(const Vector3& point) const;
        bool raycast(const Ray& ray, float& out_distance) const;
        bool intersect_ray(const Vector3& origin, const Vector3& direction, float& out_distance) const;
    };
} // namespace hob
