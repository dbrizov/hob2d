#pragma once

#include <string>

#include "vector3.h"

namespace hob {
    struct Ray {
        Vector3 origin;
        Vector3 direction = Vector3::forward();

        constexpr Ray() = default;

        Ray(const Vector3& origin_, const Vector3& direction_)
            : origin(origin_)
            , direction(direction_.normalized()) {}

        std::string to_string() const;

        Vector3 point_at(float distance) const {
            return origin + direction * distance;
        }
    };
} // namespace hob
