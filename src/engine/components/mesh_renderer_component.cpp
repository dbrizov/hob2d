#include "mesh_renderer_component.h"

#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/entity/entity.h"
#include "transform_component_3d.h"

namespace hob {
    MeshRendererComponent::MeshRendererComponent(Entity& entity)
        : Component(entity) {
        m_material = get_engine().get_renderer().get_default_mesh_material();
    }

    void MeshRendererComponent::enter_world() {
        m_mesh_draw_id = get_engine().get_renderer().register_mesh_draw();
        get_engine().get_entity_spawner().register_mesh_renderer(this);
        m_render_dirty = true;
    }

    void MeshRendererComponent::exit_world() {
        get_engine().get_entity_spawner().unregister_mesh_renderer(this);
        get_engine().get_renderer().unregister_mesh_draw(m_mesh_draw_id);
        m_mesh_draw_id = INVALID_MESH_DRAW_ID;
    }

    std::string MeshRendererComponent::to_string() const {
        return "MeshRendererComponent";
    }

    MeshDrawId MeshRendererComponent::get_mesh_draw_id() const {
        return m_mesh_draw_id;
    }

    bool MeshRendererComponent::consume_render_dirty() {
        const bool was_dirty = m_render_dirty;
        m_render_dirty = false;
        return was_dirty;
    }

    const MeshRef& MeshRendererComponent::get_mesh() const {
        return m_mesh;
    }

    void MeshRendererComponent::set_mesh(MeshRef mesh) {
        m_mesh = std::move(mesh);
        m_render_dirty = true;
    }

    const MaterialRef& MeshRendererComponent::get_material() const {
        return m_material;
    }

    MaterialRef MeshRendererComponent::get_material() {
        m_render_dirty = true;
        return m_material;
    }

    void MeshRendererComponent::set_material(MaterialRef material) {
        if (material != nullptr && material->get_shader() != nullptr &&
            material->get_shader()->get_vertex_layout() != VertexLayout::Mesh) {
            log::renderer.error("MeshRendererComponent: material '{}' uses the sprite shader '{}', not a mesh shader",
                                material->get_name(),
                                material->get_shader()->get_path());
            return;
        }

        m_material = std::move(material);
        m_render_dirty = true;
    }

    AABB3 MeshRendererComponent::get_local_bounds() const {
        return m_mesh != nullptr ? m_mesh->get_bounds() : AABB3(Vector3::zero(), Vector3::one() * 0.5f);
    }

    AABB3 MeshRendererComponent::get_world_bounds() const {
        return get_local_bounds().transformed(get_entity().get_transform_3d()->get_world_matrix());
    }
} // namespace hob
