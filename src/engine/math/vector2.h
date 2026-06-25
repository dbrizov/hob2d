#pragma once

#include <cmath>
#include <string>

#include "constants.h"
#include "engine/core/assert.h"

namespace hob {
    struct Vector2 {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vector2() = default;

        constexpr Vector2(float x_, float y_)
            : x(x_)
            , y(y_) {}

        std::string to_string() const;

        // clang-format off
        static constexpr Vector2 zero() { return Vector2(0.0f, 0.0f); }
        static constexpr Vector2 one() { return Vector2(1.0f, 1.0f); }
        static constexpr Vector2 left() { return Vector2(-1.0f, 0.0f); }
        static constexpr Vector2 right() { return Vector2(1.0f, 0.0f); }
        static constexpr Vector2 up() { return Vector2(0.0f, 1.0f); }
        static constexpr Vector2 down() { return Vector2(0.0f, -1.0f); }
        // clang-format on

        float length() const {
            return sqrtf(x * x + y * y);
        }

        float length_sqr() const {
            return x * x + y * y;
        }

        Vector2 normalized() const {
            const float len = length();
            if (len <= EPSILON) {
                return Vector2::zero();
            }

            return Vector2(x / len, y / len);
        }

        Vector2 operator+(const Vector2& right) const {
            return Vector2(x + right.x, y + right.y);
        }

        Vector2& operator+=(const Vector2& right) {
            x += right.x;
            y += right.y;
            return *this;
        }

        Vector2 operator-() const {
            return Vector2(-x, -y);
        }

        Vector2 operator-(const Vector2& right) const {
            return Vector2(x - right.x, y - right.y);
        }

        Vector2& operator-=(const Vector2& right) {
            x -= right.x;
            y -= right.y;
            return *this;
        }

        Vector2 operator*(float scalar) const {
            return Vector2(x * scalar, y * scalar);
        }

        Vector2 operator/(float scalar) const {
            HOB_ASSERT(scalar != 0.0f, "Division by zero");
            return Vector2(x / scalar, y / scalar);
        }

        bool operator==(const Vector2& right) const {
            return (std::abs(x - right.x) < EPSILON) && (std::abs(y - right.y) < EPSILON);
        }

        bool operator!=(const Vector2& right) const {
            return !operator==(right);
        }

        static float distance(const Vector2& a, const Vector2& b) {
            return (a - b).length();
        }

        static float dot(const Vector2& a, const Vector2& b) {
            return a.x * b.x + a.y * b.y;
        }

        static Vector2 lerp(const Vector2& a, const Vector2& b, float t) {
            return a * (1.0f - t) + b * t;
        }

        static Vector2 rotate_around(const Vector2& point, const Vector2& pivot, float radians);
    };
} // namespace hob
