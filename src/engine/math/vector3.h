#pragma once

#include <cmath>
#include <string>

#include "constants.h"
#include "engine/core/assert.h"
#include "mathf.h"

namespace hob {
    struct Vector3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vector3() = default;

        constexpr Vector3(float x_, float y_, float z_)
            : x(x_)
            , y(y_)
            , z(z_) {}

        std::string to_string() const;

        // clang-format off
        static constexpr Vector3 zero() { return Vector3(0.0f, 0.0f, 0.0f); }
        static constexpr Vector3 one() { return Vector3(1.0f, 1.0f, 1.0f); }
        static constexpr Vector3 left() { return Vector3(-1.0f, 0.0f, 0.0f); }
        static constexpr Vector3 right() { return Vector3(1.0f, 0.0f, 0.0f); }
        static constexpr Vector3 up() { return Vector3(0.0f, 1.0f, 0.0f); }
        static constexpr Vector3 down() { return Vector3(0.0f, -1.0f, 0.0f); }
        static constexpr Vector3 forward() { return Vector3(0.0f, 0.0f, 1.0f); }
        static constexpr Vector3 back() { return Vector3(0.0f, 0.0f, -1.0f); }
        // clang-format on

        float length() const {
            return sqrtf(x * x + y * y + z * z);
        }

        float length_sqr() const {
            return x * x + y * y + z * z;
        }

        Vector3 normalized() const {
            const float len = length();
            if (len <= EPSILON) {
                return Vector3::zero();
            }

            return Vector3(x / len, y / len, z / len);
        }

        Vector3 operator+(const Vector3& right) const {
            return Vector3(x + right.x, y + right.y, z + right.z);
        }

        Vector3& operator+=(const Vector3& right) {
            x += right.x;
            y += right.y;
            z += right.z;
            return *this;
        }

        Vector3 operator-() const {
            return Vector3(-x, -y, -z);
        }

        Vector3 operator-(const Vector3& right) const {
            return Vector3(x - right.x, y - right.y, z - right.z);
        }

        Vector3& operator-=(const Vector3& right) {
            x -= right.x;
            y -= right.y;
            z -= right.z;
            return *this;
        }

        Vector3 operator*(float scalar) const {
            return Vector3(x * scalar, y * scalar, z * scalar);
        }

        Vector3& operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        Vector3 operator/(float scalar) const {
            HOB_ASSERT(scalar != 0.0f, "Division by zero");
            return Vector3(x / scalar, y / scalar, z / scalar);
        }

        bool operator==(const Vector3& right) const {
            return math::approx_equal(x, right.x) && math::approx_equal(y, right.y) && math::approx_equal(z, right.z);
        }

        bool operator!=(const Vector3& right) const {
            return !operator==(right);
        }

        static float distance(const Vector3& a, const Vector3& b) {
            return (a - b).length();
        }

        static float dot(const Vector3& a, const Vector3& b) {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        static Vector3 cross(const Vector3& a, const Vector3& b) {
            return Vector3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
        }

        static Vector3 scale(const Vector3& a, const Vector3& b) {
            return Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
        }

        static Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
            return a * (1.0f - t) + b * t;
        }

        static Vector3 min(const Vector3& a, const Vector3& b) {
            return Vector3(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z));
        }

        static Vector3 max(const Vector3& a, const Vector3& b) {
            return Vector3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z));
        }

        static Vector3 abs(const Vector3& a) {
            return Vector3(std::abs(a.x), std::abs(a.y), std::abs(a.z));
        }

        static Vector3 project_on_plane(const Vector3& vector, const Vector3& plane_normal) {
            return vector - plane_normal * dot(vector, plane_normal);
        }
    };
} // namespace hob
