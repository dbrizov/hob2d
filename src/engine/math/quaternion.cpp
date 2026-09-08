#include "quaternion.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "constants.h"

namespace hob {
    namespace {
        constexpr float GIMBAL_LOCK_THRESHOLD = 0.9999f;
    } // namespace

    Quaternion Quaternion::from_basis(const Vector3& right, const Vector3& up, const Vector3& forward) {
        const float trace = right.x + up.y + forward.z;
        Quaternion q;
        if (trace > 0.0f) {
            const float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (up.z - forward.y) / s;
            q.y = (forward.x - right.z) / s;
            q.z = (right.y - up.x) / s;
        }
        else if (right.x > up.y && right.x > forward.z) {
            const float s = std::sqrt(1.0f + right.x - up.y - forward.z) * 2.0f;
            q.w = (up.z - forward.y) / s;
            q.x = 0.25f * s;
            q.y = (up.x + right.y) / s;
            q.z = (forward.x + right.z) / s;
        }
        else if (up.y > forward.z) {
            const float s = std::sqrt(1.0f + up.y - right.x - forward.z) * 2.0f;
            q.w = (forward.x - right.z) / s;
            q.x = (up.x + right.y) / s;
            q.y = 0.25f * s;
            q.z = (forward.y + up.z) / s;
        }
        else {
            const float s = std::sqrt(1.0f + forward.z - right.x - up.y) * 2.0f;
            q.w = (right.y - up.x) / s;
            q.x = (forward.x + right.z) / s;
            q.y = (forward.y + up.z) / s;
            q.z = 0.25f * s;
        }

        return q.normalized();
    }

    std::string Quaternion::to_string() const {
        return std::format("({:.3f}, {:.3f}, {:.3f}, {:.3f})", x, y, z, w);
    }

    Quaternion Quaternion::from_axis_angle(const Vector3& axis, float radians) {
        const Vector3 unit_axis = axis.normalized();
        const float half = radians * 0.5f;
        const float s = std::sin(half);
        return Quaternion(unit_axis.x * s, unit_axis.y * s, unit_axis.z * s, std::cos(half));
    }

    Quaternion Quaternion::from_euler_deg(const Vector3& euler_deg) {
        const Quaternion around_x = from_axis_angle(Vector3::right(), euler_deg.x * DEG_TO_RAD);
        const Quaternion around_y = from_axis_angle(Vector3::up(), euler_deg.y * DEG_TO_RAD);
        const Quaternion around_z = from_axis_angle(Vector3::forward(), euler_deg.z * DEG_TO_RAD);
        return around_y * around_x * around_z;
    }

    Vector3 Quaternion::to_euler_deg() const {
        const Vector3 right = get_right();
        const Vector3 up = get_up();
        const Vector3 forward = get_forward();

        const float sin_x = std::clamp(-forward.y, -1.0f, 1.0f);
        const float x_rad = std::asin(sin_x);

        float y_rad;
        float z_rad;
        if (std::abs(sin_x) < GIMBAL_LOCK_THRESHOLD) {
            y_rad = std::atan2(forward.x, forward.z);
            z_rad = std::atan2(right.y, up.y);
        }
        else {
            y_rad = std::atan2(sin_x * up.x, right.x);
            z_rad = 0.0f;
        }

        return Vector3(x_rad * RAD_TO_DEG, y_rad * RAD_TO_DEG, z_rad * RAD_TO_DEG);
    }

    Quaternion Quaternion::look_rotation(const Vector3& forward, const Vector3& up) {
        const Vector3 z = forward.normalized();
        if (z == Vector3::zero()) {
            return identity();
        }

        Vector3 x = Vector3::cross(up, z);
        if (x.length_sqr() <= EPSILON) {
            const Vector3 fallback_up = std::abs(z.y) < GIMBAL_LOCK_THRESHOLD ? Vector3::up() : Vector3::forward();
            x = Vector3::cross(fallback_up, z);
        }
        x = x.normalized();
        const Vector3 y = Vector3::cross(z, x);

        return from_basis(x, y, z);
    }

    Quaternion Quaternion::from_to_rotation(const Vector3& from, const Vector3& to) {
        const Vector3 a = from.normalized();
        const Vector3 b = to.normalized();
        const float cos_angle = Vector3::dot(a, b);
        if (cos_angle >= 1.0f - EPSILON) {
            return identity();
        }

        if (cos_angle <= -1.0f + EPSILON) {
            Vector3 axis = Vector3::cross(Vector3::right(), a);
            if (axis.length_sqr() <= EPSILON) {
                axis = Vector3::cross(Vector3::up(), a);
            }
            return from_axis_angle(axis, PI);
        }

        const Vector3 axis = Vector3::cross(a, b);
        const float s = std::sqrt((1.0f + cos_angle) * 2.0f);
        return Quaternion(axis.x / s, axis.y / s, axis.z / s, s * 0.5f).normalized();
    }

    float Quaternion::length() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    Quaternion Quaternion::normalized() const {
        const float len = length();
        if (len <= EPSILON) {
            return identity();
        }

        return Quaternion(x / len, y / len, z / len, w / len);
    }

    Quaternion Quaternion::conjugate() const {
        return Quaternion(-x, -y, -z, w);
    }

    Quaternion Quaternion::inverse() const {
        const float len_sqr = x * x + y * y + z * z + w * w;
        if (len_sqr <= EPSILON) {
            return identity();
        }

        return Quaternion(-x / len_sqr, -y / len_sqr, -z / len_sqr, w / len_sqr);
    }

    Vector3 Quaternion::rotate(const Vector3& vector) const {
        const Vector3 u(x, y, z);
        const Vector3 t = Vector3::cross(u, vector) * 2.0f;
        return vector + t * w + Vector3::cross(u, t);
    }

    Vector3 Quaternion::get_forward() const {
        return rotate(Vector3::forward());
    }

    Vector3 Quaternion::get_right() const {
        return rotate(Vector3::right());
    }

    Vector3 Quaternion::get_up() const {
        return rotate(Vector3::up());
    }

    Quaternion Quaternion::operator*(const Quaternion& right) const {
        return Quaternion(w * right.x + x * right.w + y * right.z - z * right.y,
                          w * right.y - x * right.z + y * right.w + z * right.x,
                          w * right.z + x * right.y - y * right.x + z * right.w,
                          w * right.w - x * right.x - y * right.y - z * right.z);
    }

    Quaternion& Quaternion::operator*=(const Quaternion& right) {
        *this = *this * right;
        return *this;
    }

    float Quaternion::dot(const Quaternion& a, const Quaternion& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    Quaternion Quaternion::slerp(const Quaternion& a, const Quaternion& b, float t) {
        float cos_theta = dot(a, b);
        Quaternion end = b;
        if (cos_theta < 0.0f) {
            cos_theta = -cos_theta;
            end = Quaternion(-b.x, -b.y, -b.z, -b.w);
        }

        float weight_a;
        float weight_b;
        if (cos_theta > GIMBAL_LOCK_THRESHOLD) {
            weight_a = 1.0f - t;
            weight_b = t;
        }
        else {
            const float theta = std::acos(cos_theta);
            const float sin_theta = std::sin(theta);
            weight_a = std::sin((1.0f - t) * theta) / sin_theta;
            weight_b = std::sin(t * theta) / sin_theta;
        }

        return Quaternion(a.x * weight_a + end.x * weight_b,
                          a.y * weight_a + end.y * weight_b,
                          a.z * weight_a + end.z * weight_b,
                          a.w * weight_a + end.w * weight_b)
            .normalized();
    }

    float Quaternion::angle_deg(const Quaternion& a, const Quaternion& b) {
        const float cos_half = std::clamp(std::abs(dot(a, b)), 0.0f, 1.0f);
        return std::acos(cos_half) * 2.0f * RAD_TO_DEG;
    }
} // namespace hob
