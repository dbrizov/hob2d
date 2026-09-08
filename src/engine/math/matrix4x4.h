#pragma once

#include <array>
#include <string>

#include "quaternion.h"
#include "vector3.h"

namespace hob {
    // Column-major (translation in elements 12,13,14), matching the layout the renderer uploads to HLSL cbuffers.
    struct Matrix4x4 {
        std::array<float, 16> m{};

        const float* data() const {
            return m.data();
        }

        float* data() {
            return m.data();
        }

        static constexpr uint32_t byte_size() {
            return static_cast<uint32_t>(16 * sizeof(float));
        }

        float at(int32_t row, int32_t col) const {
            return m[col * 4 + row];
        }

        float& at(int32_t row, int32_t col) {
            return m[col * 4 + row];
        }

        std::string to_string() const;

        static Matrix4x4 identity();
        static Matrix4x4 translation(const Vector3& translation);
        static Matrix4x4 rotation(const Quaternion& rotation);
        static Matrix4x4 scaling(const Vector3& scale);
        static Matrix4x4 trs(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

        static Matrix4x4 perspective_lh(float fov_y_rad, float aspect, float near_plane, float far_plane);
        static Matrix4x4 orthographic_lh(float width, float height, float near_plane, float far_plane);
        static Matrix4x4 look_at_lh(const Vector3& eye, const Vector3& target, const Vector3& up = Vector3::up());

        // Column-major matrix product (result = a * b): applies b first, then a.
        static Matrix4x4 multiply(const Matrix4x4& a, const Matrix4x4& b);

        Matrix4x4 transpose() const;
        Matrix4x4 inverse() const;
        Matrix4x4 inverse_affine() const;
        float determinant() const;

        Vector3 transform_point(const Vector3& point) const;
        Vector3 transform_direction(const Vector3& direction) const;
        bool project_point(const Vector3& point, Vector3& out_ndc) const;

        Vector3 get_translation() const;
        Vector3 get_column3(int32_t col) const;
        Vector3 get_scale() const;
        Quaternion get_rotation() const;
        void decompose(Vector3& out_translation, Quaternion& out_rotation, Vector3& out_scale) const;
    };

    inline Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
        return Matrix4x4::multiply(a, b);
    }
} // namespace hob
