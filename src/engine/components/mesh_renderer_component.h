#pragma once

#include <limits>
#include <string>

#include "component.h"
#include "engine/core/systems/renderer/material.h"
#include "engine/core/systems/renderer/mesh.h"
#include "engine/core/systems/renderer/mesh_draw_data.h"
#include "engine/math/aabb3.h"

namespace hob {
    using MeshRendererIndex = uint32_t;
    constexpr MeshRendererIndex INVALID_MESH_RENDERER_INDEX = std::numeric_limits<MeshRendererIndex>::max();

    class MeshRendererComponent : public Component {
        friend class EntitySpawner;

        MeshRef m_mesh;
        MaterialRef m_material;
        MeshDrawId m_mesh_draw_id = INVALID_MESH_DRAW_ID;
        MeshRendererIndex m_mesh_renderer_index = INVALID_MESH_RENDERER_INDEX;
        bool m_render_dirty = true;

    public:
        explicit MeshRendererComponent(Entity& entity);

        void enter_world() override;
        void exit_world() override;

        std::string to_string() const override;

        MeshDrawId get_mesh_draw_id() const;
        bool consume_render_dirty();

        const MeshRef& get_mesh() const;
        void set_mesh(MeshRef mesh);

        const MaterialRef& get_material() const;
        MaterialRef get_material();
        void set_material(MaterialRef material);

        AABB3 get_local_bounds() const;
        AABB3 get_world_bounds() const;
    };
} // namespace hob
