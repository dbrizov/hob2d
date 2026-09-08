#include "matrix4x4.h"

#include <cmath>
#include <format>

#include "constants.h"

namespace hob {
    std::string Matrix4x4::to_string() const {
        return std::format("[{:.3f} {:.3f} {:.3f} {:.3f}; {:.3f} {:.3f} {:.3f} {:.3f}; {:.3f} {:.3f} {:.3f} {:.3f}; "
                           "{:.3f} {:.3f} {:.3f} {:.3f}]",
                           at(0, 0),
                           at(0, 1),
                           at(0, 2),
                           at(0, 3),
                           at(1, 0),
                           at(1, 1),
                           at(1, 2),
                           at(1, 3),
                           at(2, 0),
                           at(2, 1),
                           at(2, 2),
                           at(2, 3),
                           at(3, 0),
                           at(3, 1),
                           at(3, 2),
                           at(3, 3));
    }

    Matrix4x4 Matrix4x4::identity() {
        Matrix4x4 out;
        out.m[0] = 1.0f;
        out.m[5] = 1.0f;
        out.m[10] = 1.0f;
        out.m[15] = 1.0f;
        return out;
    }

    Matrix4x4 Matrix4x4::translation(const Vector3& translation) {
        Matrix4x4 out = identity();
        out.m[12] = translation.x;
        out.m[13] = translation.y;
        out.m[14] = translation.z;
        return out;
    }

    Matrix4x4 Matrix4x4::rotation(const Quaternion& rotation) {
        const Quaternion q = rotation.normalized();
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        Matrix4x4 out = identity();
        out.m[0] = 1.0f - 2.0f * (yy + zz);
        out.m[1] = 2.0f * (xy + wz);
        out.m[2] = 2.0f * (xz - wy);

        out.m[4] = 2.0f * (xy - wz);
        out.m[5] = 1.0f - 2.0f * (xx + zz);
        out.m[6] = 2.0f * (yz + wx);

        out.m[8] = 2.0f * (xz + wy);
        out.m[9] = 2.0f * (yz - wx);
        out.m[10] = 1.0f - 2.0f * (xx + yy);
        return out;
    }

    Matrix4x4 Matrix4x4::scaling(const Vector3& scale) {
        Matrix4x4 out = identity();
        out.m[0] = scale.x;
        out.m[5] = scale.y;
        out.m[10] = scale.z;
        return out;
    }

    Matrix4x4 Matrix4x4::trs(const Vector3& translation, const Quaternion& rotation, const Vector3& scale) {
        Matrix4x4 out = Matrix4x4::rotation(rotation);
        for (int32_t row = 0; row < 3; ++row) {
            out.at(row, 0) *= scale.x;
            out.at(row, 1) *= scale.y;
            out.at(row, 2) *= scale.z;
        }
        out.m[12] = translation.x;
        out.m[13] = translation.y;
        out.m[14] = translation.z;
        return out;
    }

    Matrix4x4 Matrix4x4::perspective_lh(float fov_y_rad, float aspect, float near_plane, float far_plane) {
        const float focal = 1.0f / std::tan(fov_y_rad * 0.5f);
        const float depth_range = far_plane - near_plane;

        Matrix4x4 out;
        out.m[0] = focal / aspect;
        out.m[5] = focal;
        out.m[10] = far_plane / depth_range;
        out.m[11] = 1.0f;
        out.m[14] = -near_plane * far_plane / depth_range;
        return out;
    }

    Matrix4x4 Matrix4x4::orthographic_lh(float width, float height, float near_plane, float far_plane) {
        const float depth_range = far_plane - near_plane;

        Matrix4x4 out;
        out.m[0] = 2.0f / width;
        out.m[5] = 2.0f / height;
        out.m[10] = 1.0f / depth_range;
        out.m[14] = -near_plane / depth_range;
        out.m[15] = 1.0f;
        return out;
    }

    Matrix4x4 Matrix4x4::look_at_lh(const Vector3& eye, const Vector3& target, const Vector3& up) {
        const Vector3 z = (target - eye).normalized();
        const Vector3 x = Vector3::cross(up, z).normalized();
        const Vector3 y = Vector3::cross(z, x);

        Matrix4x4 out = identity();
        out.at(0, 0) = x.x;
        out.at(0, 1) = x.y;
        out.at(0, 2) = x.z;
        out.at(1, 0) = y.x;
        out.at(1, 1) = y.y;
        out.at(1, 2) = y.z;
        out.at(2, 0) = z.x;
        out.at(2, 1) = z.y;
        out.at(2, 2) = z.z;
        out.at(0, 3) = -Vector3::dot(x, eye);
        out.at(1, 3) = -Vector3::dot(y, eye);
        out.at(2, 3) = -Vector3::dot(z, eye);
        return out;
    }

    Matrix4x4 Matrix4x4::multiply(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 out;
        for (int32_t col = 0; col < 4; ++col) {
            for (int32_t row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int32_t k = 0; k < 4; ++k) {
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                }
                out.m[col * 4 + row] = sum;
            }
        }

        return out;
    }

    Matrix4x4 Matrix4x4::transpose() const {
        Matrix4x4 out;
        for (int32_t col = 0; col < 4; ++col) {
            for (int32_t row = 0; row < 4; ++row) {
                out.at(row, col) = at(col, row);
            }
        }
        return out;
    }

    float Matrix4x4::determinant() const {
        const float* a = m.data();
        const float sub0 = a[10] * a[15] - a[14] * a[11];
        const float sub1 = a[9] * a[15] - a[13] * a[11];
        const float sub2 = a[9] * a[14] - a[13] * a[10];
        const float sub3 = a[8] * a[15] - a[12] * a[11];
        const float sub4 = a[8] * a[14] - a[12] * a[10];
        const float sub5 = a[8] * a[13] - a[12] * a[9];

        const float cof0 = a[5] * sub0 - a[6] * sub1 + a[7] * sub2;
        const float cof1 = a[4] * sub0 - a[6] * sub3 + a[7] * sub4;
        const float cof2 = a[4] * sub1 - a[5] * sub3 + a[7] * sub5;
        const float cof3 = a[4] * sub2 - a[5] * sub4 + a[6] * sub5;

        return a[0] * cof0 - a[1] * cof1 + a[2] * cof2 - a[3] * cof3;
    }

    Matrix4x4 Matrix4x4::inverse() const {
        const float* a = m.data();
        std::array<float, 16> inv;

        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
                 a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
                 a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
                 a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
                  a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
                 a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
                 a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
                 a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
                  a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
                 a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
                 a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
                  a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
                  a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
                 a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
                 a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
                  a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] +
                  a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

        const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (std::abs(det) <= EPSILON * EPSILON) {
            return identity();
        }

        const float inv_det = 1.0f / det;
        Matrix4x4 out;
        for (int32_t i = 0; i < 16; ++i) {
            out.m[i] = inv[i] * inv_det;
        }
        return out;
    }

    Matrix4x4 Matrix4x4::inverse_affine() const {
        const Vector3 c0 = get_column3(0);
        const Vector3 c1 = get_column3(1);
        const Vector3 c2 = get_column3(2);

        const Vector3 r0 = Vector3::cross(c1, c2);
        const Vector3 r1 = Vector3::cross(c2, c0);
        const Vector3 r2 = Vector3::cross(c0, c1);
        const float det = Vector3::dot(c0, r0);
        if (std::abs(det) <= EPSILON * EPSILON) {
            return identity();
        }

        const float inv_det = 1.0f / det;
        Matrix4x4 out = identity();
        out.at(0, 0) = r0.x * inv_det;
        out.at(0, 1) = r0.y * inv_det;
        out.at(0, 2) = r0.z * inv_det;
        out.at(1, 0) = r1.x * inv_det;
        out.at(1, 1) = r1.y * inv_det;
        out.at(1, 2) = r1.z * inv_det;
        out.at(2, 0) = r2.x * inv_det;
        out.at(2, 1) = r2.y * inv_det;
        out.at(2, 2) = r2.z * inv_det;

        const Vector3 t = get_translation();
        const Vector3 inv_t = out.transform_direction(t);
        out.m[12] = -inv_t.x;
        out.m[13] = -inv_t.y;
        out.m[14] = -inv_t.z;
        return out;
    }

    Vector3 Matrix4x4::transform_point(const Vector3& p) const {
        return Vector3(at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3),
                       at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3),
                       at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3));
    }

    Vector3 Matrix4x4::transform_direction(const Vector3& d) const {
        return Vector3(at(0, 0) * d.x + at(0, 1) * d.y + at(0, 2) * d.z,
                       at(1, 0) * d.x + at(1, 1) * d.y + at(1, 2) * d.z,
                       at(2, 0) * d.x + at(2, 1) * d.y + at(2, 2) * d.z);
    }

    bool Matrix4x4::project_point(const Vector3& p, Vector3& out_ndc) const {
        const float w = at(3, 0) * p.x + at(3, 1) * p.y + at(3, 2) * p.z + at(3, 3);
        if (w <= EPSILON) {
            return false;
        }

        out_ndc = transform_point(p) / w;
        return true;
    }

    Vector3 Matrix4x4::get_translation() const {
        return Vector3(m[12], m[13], m[14]);
    }

    Vector3 Matrix4x4::get_column3(int32_t col) const {
        return Vector3(at(0, col), at(1, col), at(2, col));
    }

    Vector3 Matrix4x4::get_scale() const {
        const float sign_x = determinant() < 0.0f ? -1.0f : 1.0f;
        return Vector3(sign_x * get_column3(0).length(), get_column3(1).length(), get_column3(2).length());
    }

    Quaternion Matrix4x4::get_rotation() const {
        const Vector3 scale = get_scale();
        const Vector3 right = get_column3(0) / (scale.x != 0.0f ? scale.x : 1.0f);
        const Vector3 up = get_column3(1) / (scale.y != 0.0f ? scale.y : 1.0f);
        const Vector3 forward = get_column3(2) / (scale.z != 0.0f ? scale.z : 1.0f);
        return Quaternion::from_basis(right, up, forward);
    }

    void Matrix4x4::decompose(Vector3& out_translation, Quaternion& out_rotation, Vector3& out_scale) const {
        out_translation = get_translation();
        out_scale = get_scale();
        out_rotation = get_rotation();
    }
} // namespace hob
