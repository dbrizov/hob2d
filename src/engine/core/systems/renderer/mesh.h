#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "engine/core/asset.h"
#include "engine/math/aabb3.h"
#include "engine/math/vector2.h"
#include "engine/math/vector3.h"

namespace hob {
    struct MeshVertex {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;
        Vector3 tangent;
        float tangent_sign = 1.0f;
    };

    static_assert(sizeof(MeshVertex) == 48);

    struct MeshData {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        AABB3 bounds;

        void recompute_bounds();
        void compute_tangents();
    };

    class Mesh;
    using MeshRef = std::shared_ptr<Mesh>;
    using MeshWeakRef = std::weak_ptr<Mesh>;

    class Mesh : public Asset {
        friend class Renderer;

        SDL_GPUDevice* m_device = nullptr;
        SDL_GPUBuffer* m_vertex_buffer = nullptr;
        SDL_GPUBuffer* m_index_buffer = nullptr;
        uint32_t m_vertex_count = 0;
        uint32_t m_index_count = 0;
        AABB3 m_bounds;
        std::string m_source;

        Mesh(SDL_GPUDevice* device,
             SDL_GPUBuffer* vertex_buffer,
             SDL_GPUBuffer* index_buffer,
             uint32_t vertex_count,
             uint32_t index_count,
             const AABB3& bounds,
             std::string source);

    public:
        ~Mesh() override;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) = delete;
        Mesh& operator=(Mesh&&) = delete;

        SDL_GPUBuffer* get_vertex_buffer() const;
        SDL_GPUBuffer* get_index_buffer() const;
        uint32_t get_vertex_count() const;
        uint32_t get_index_count() const;
        const AABB3& get_bounds() const;
        const std::string& get_source() const;
    };
} // namespace hob
