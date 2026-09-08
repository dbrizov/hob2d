#pragma once

#include <string>

#include "mathf.h"
#include "vector3.h"

namespace hob {
    struct Quaternion {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        constexpr Quaternion() = default;

        constexpr Quaternion(float x_, float y_, float z_, float w_)
            : x(x_)
            , y(y_)
            , z(z_)
            , w(w_) {}

        std::string to_string() const;

        static constexpr Quaternion identity() {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        static Quaternion from_axis_angle(const Vector3& axis, float radians);

        // Z-X-Y order: applies Z first, then X, then Y (Unity's convention).
        static Quaternion from_euler_deg(const Vector3& euler_deg);
        Vector3 to_euler_deg() const;

        static Quaternion from_basis(const Vector3& right, const Vector3& up, const Vector3& forward);
        static Quaternion look_rotation(const Vector3& forward, const Vector3& up = Vector3::up());
        static Quaternion from_to_rotation(const Vector3& from, const Vector3& to);

        float length() const;
        Quaternion normalized() const;
        Quaternion conjugate() const;
        Quaternion inverse() const;

        Vector3 rotate(const Vector3& vector) const;
        Vector3 get_forward() const;
        Vector3 get_right() const;
        Vector3 get_up() const;

        // (a * b) applies b first, then a.
        Quaternion operator*(const Quaternion& right) const;
        Quaternion& operator*=(const Quaternion& right);

        bool operator==(const Quaternion& right) const {
            return math::approx_equal(x, right.x) && math::approx_equal(y, right.y) && math::approx_equal(z, right.z) &&
                   math::approx_equal(w, right.w);
        }

        bool operator!=(const Quaternion& right) const {
            return !operator==(right);
        }

        static float dot(const Quaternion& a, const Quaternion& b);
        static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t);
        static float angle_deg(const Quaternion& a, const Quaternion& b);
    };
} // namespace hob
