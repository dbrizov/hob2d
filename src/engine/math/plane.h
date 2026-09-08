#pragma once

#include <cmath>

#include "constants.h"
#include "ray.h"
#include "vector3.h"

namespace hob {
    struct Plane {
        Vector3 normal = Vector3::up();
        float distance = 0.0f;

        constexpr Plane() = default;

        Plane(const Vector3& normal_, float distance_)
            : normal(normal_.normalized())
            , distance(distance_) {}

        static Plane from_point_normal(const Vector3& point, const Vector3& normal) {
            const Vector3 unit_normal = normal.normalized();
            return Plane(unit_normal, Vector3::dot(unit_normal, point));
        }

        float signed_distance(const Vector3& point) const {
            return Vector3::dot(normal, point) - distance;
        }

        bool raycast(const Ray& ray, float& out_distance) const {
            const float denominator = Vector3::dot(normal, ray.direction);
            if (std::abs(denominator) <= EPSILON) {
                return false;
            }

            const float t = (distance - Vector3::dot(normal, ray.origin)) / denominator;
            if (t < 0.0f) {
                return false;
            }

            out_distance = t;
            return true;
        }
    };
} // namespace hob
