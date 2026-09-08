#include "mesh.h"

#include <cmath>
#include <utility>

namespace hob {
    void MeshData::recompute_bounds() {
        if (vertices.empty()) {
            bounds = AABB3();
            return;
        }

        Vector3 lo = vertices[0].position;
        Vector3 hi = lo;
        for (const MeshVertex& vertex : vertices) {
            lo = Vector3::min(lo, vertex.position);
            hi = Vector3::max(hi, vertex.position);
        }

        bounds = AABB3::from_min_max(lo, hi);
    }

    void MeshData::compute_tangents() {
        std::vector<Vector3> tangents(vertices.size(), Vector3::zero());
        std::vector<Vector3> bitangents(vertices.size(), Vector3::zero());

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const uint32_t ia = indices[i];
            const uint32_t ib = indices[i + 1];
            const uint32_t ic = indices[i + 2];
            const MeshVertex& a = vertices[ia];
            const MeshVertex& b = vertices[ib];
            const MeshVertex& c = vertices[ic];

            const Vector3 edge1 = b.position - a.position;
            const Vector3 edge2 = c.position - a.position;
            const Vector2 delta_uv1 = b.uv - a.uv;
            const Vector2 delta_uv2 = c.uv - a.uv;

            const float determinant = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
            if (std::fabs(determinant) < 1.0e-8f) {
                continue;
            }

            const float r = 1.0f / determinant;
            const Vector3 tangent = (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * r;
            const Vector3 bitangent = (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * r;
            for (const uint32_t index : {ia, ib, ic}) {
                tangents[index] += tangent;
                bitangents[index] += bitangent;
            }
        }

        for (size_t i = 0; i < vertices.size(); ++i) {
            MeshVertex& vertex = vertices[i];
            const Vector3& n = vertex.normal;
            Vector3 t = tangents[i] - n * Vector3::dot(n, tangents[i]);
            if (t.length_sqr() < 1.0e-12f) {
                const Vector3 axis = std::fabs(n.x) < 0.9f ? Vector3::right() : Vector3::up();
                t = Vector3::cross(axis, n);
            }

            vertex.tangent = t.normalized();
            vertex.tangent_sign = Vector3::dot(Vector3::cross(n, vertex.tangent), bitangents[i]) < 0.0f ? -1.0f : 1.0f;
        }
    }

    Mesh::Mesh(SDL_GPUDevice* device,
               SDL_GPUBuffer* vertex_buffer,
               SDL_GPUBuffer* index_buffer,
               uint32_t vertex_count,
               uint32_t index_count,
               const AABB3& bounds,
               std::string source)
        : m_device(device)
        , m_vertex_buffer(vertex_buffer)
        , m_index_buffer(index_buffer)
        , m_vertex_count(vertex_count)
        , m_index_count(index_count)
        , m_bounds(bounds)
        , m_source(std::move(source)) {}

    Mesh::~Mesh() {
        if (m_index_buffer != nullptr) {
            SDL_ReleaseGPUBuffer(m_device, m_index_buffer);
        }

        if (m_vertex_buffer != nullptr) {
            SDL_ReleaseGPUBuffer(m_device, m_vertex_buffer);
        }
    }

    SDL_GPUBuffer* Mesh::get_vertex_buffer() const {
        return m_vertex_buffer;
    }

    SDL_GPUBuffer* Mesh::get_index_buffer() const {
        return m_index_buffer;
    }

    uint32_t Mesh::get_vertex_count() const {
        return m_vertex_count;
    }

    uint32_t Mesh::get_index_count() const {
        return m_index_count;
    }

    const AABB3& Mesh::get_bounds() const {
        return m_bounds;
    }

    const std::string& Mesh::get_source() const {
        return m_source;
    }
} // namespace hob
