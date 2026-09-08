#include "mesh_primitives.h"

#include <cmath>

#include "engine/math/constants.h"

namespace hob::mesh_primitives {
    namespace {
        void append_quad(MeshData& data,
                         const Vector3& center,
                         const Vector3& half_u,
                         const Vector3& half_v,
                         const Vector3& normal) {
            const uint32_t base = static_cast<uint32_t>(data.vertices.size());
            data.vertices.push_back({center - half_u - half_v, normal, Vector2(0.0f, 1.0f)});
            data.vertices.push_back({center + half_u - half_v, normal, Vector2(1.0f, 1.0f)});
            data.vertices.push_back({center + half_u + half_v, normal, Vector2(1.0f, 0.0f)});
            data.vertices.push_back({center - half_u + half_v, normal, Vector2(0.0f, 0.0f)});

            data.indices.insert(data.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
        }

        void append_lathe(MeshData& data,
                          uint32_t rings,
                          uint32_t segments,
                          float radius,
                          float half_height,
                          float ring_start,
                          float ring_end) {
            const uint32_t base = static_cast<uint32_t>(data.vertices.size());
            for (uint32_t ring = 0; ring <= rings; ++ring) {
                const float t = static_cast<float>(ring) / static_cast<float>(rings);
                const float phi = (ring_start + (ring_end - ring_start) * t) * PI;
                const float y = std::cos(phi);
                const float r = std::sin(phi);
                const float y_offset = phi <= PI * 0.5f ? half_height : -half_height;
                for (uint32_t segment = 0; segment <= segments; ++segment) {
                    const float s = static_cast<float>(segment) / static_cast<float>(segments);
                    const float theta = s * PI * 2.0f;
                    const Vector3 normal(r * std::sin(theta), y, r * std::cos(theta));
                    const Vector3 position = normal * radius + Vector3(0.0f, y_offset, 0.0f);
                    data.vertices.push_back({position, normal, Vector2(s, t)});
                }
            }

            const uint32_t stride = segments + 1;
            for (uint32_t ring = 0; ring < rings; ++ring) {
                for (uint32_t segment = 0; segment < segments; ++segment) {
                    const uint32_t a = base + ring * stride + segment;
                    const uint32_t b = a + stride;
                    data.indices.insert(data.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
                }
            }
        }
    } // namespace

    MeshData make_cube() {
        MeshData data;
        const float h = 0.5f;
        append_quad(data, Vector3(0, 0, -h), Vector3(h, 0, 0), Vector3(0, h, 0), Vector3::back());
        append_quad(data, Vector3(0, 0, h), Vector3(-h, 0, 0), Vector3(0, h, 0), Vector3::forward());
        append_quad(data, Vector3(-h, 0, 0), Vector3(0, 0, -h), Vector3(0, h, 0), Vector3::left());
        append_quad(data, Vector3(h, 0, 0), Vector3(0, 0, h), Vector3(0, h, 0), Vector3::right());
        append_quad(data, Vector3(0, h, 0), Vector3(h, 0, 0), Vector3(0, 0, h), Vector3::up());
        append_quad(data, Vector3(0, -h, 0), Vector3(h, 0, 0), Vector3(0, 0, -h), Vector3::down());
        data.compute_tangents();
        data.recompute_bounds();
        return data;
    }

    MeshData make_plane() {
        MeshData data;
        append_quad(data, Vector3::zero(), Vector3(0.5f, 0, 0), Vector3(0, 0, 0.5f), Vector3::up());
        data.compute_tangents();
        data.recompute_bounds();
        return data;
    }

    MeshData make_sphere(uint32_t rings, uint32_t segments) {
        MeshData data;
        append_lathe(data, rings, segments, 0.5f, 0.0f, 0.0f, 1.0f);
        data.compute_tangents();
        data.recompute_bounds();
        return data;
    }

    MeshData make_capsule(float radius, float height, uint32_t rings, uint32_t segments) {
        MeshData data;
        const float half_height = std::fmax(height * 0.5f - radius, 0.0f);
        append_lathe(data, rings, segments, radius, half_height, 0.0f, 0.5f);
        append_lathe(data, rings, segments, radius, half_height, 0.5f, 1.0f);
        data.compute_tangents();
        data.recompute_bounds();
        return data;
    }

    std::optional<MeshData> make_by_name(std::string_view name) {
        if (name == CUBE) {
            return make_cube();
        }

        if (name == PLANE) {
            return make_plane();
        }

        if (name == SPHERE) {
            return make_sphere();
        }

        if (name == CAPSULE) {
            return make_capsule();
        }

        return std::nullopt;
    }
} // namespace hob::mesh_primitives
