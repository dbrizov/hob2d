#include "entity_spawner.h"

#include <cstring>

#include <imgui.h>

#include "engine/components/audio_component.h"
#include "engine/components/mesh_renderer_component.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/physics_3d/rigidbody_component_3d.h"
#include "engine/components/sprite_component.h"
#include "engine/components/transform_component.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/assert.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/console.h"
#include "engine/entity/entity.h"
#include "engine/math/constants.h"

namespace hob {
    EntitySpawner::EntitySpawner(Engine& engine)
        : m_engine(engine) {
        log::engine.info("EntitySpawner::Initialise");
    }

    EntitySpawner::~EntitySpawner() {
        clear();
        log::engine.info("EntitySpawner::Shutdown");
    }

    void EntitySpawner::set_entity_spawned_handler(std::function<void(EntityId)> callback) {
        m_entity_spawned_handler = std::move(callback);
    }

    void EntitySpawner::set_entity_destroyed_handler(std::function<void(EntityId)> callback) {
        m_entity_destroyed_handler = std::move(callback);
    }

    void EntitySpawner::set_entities_cleared_handler(std::function<void()> callback) {
        m_entities_cleared_handler = std::move(callback);
    }

    Entity& EntitySpawner::spawn_entity(Space space) {
        std::unique_ptr<Entity> entity = std::unique_ptr<Entity>(new Entity(m_engine));

        const EntityId entity_id = m_next_entity_id;
        m_next_entity_id += 1;
        entity->set_id(entity_id);
        entity->m_space = space;
        if (space == Space::Space3D) {
            entity->add_component<TransformComponent3D>();
        }
        else {
            entity->add_component<TransformComponent>();
        }

        Entity* entity_ptr = entity.get();
        m_entity_spawn_requests.push_back(std::move(entity));
        m_entity_records[entity_id] = EntityRecord{entity_ptr, INVALID_ENTITY_INDEX};

        return *entity_ptr;
    }

    void EntitySpawner::destroy_entity(EntityId id) {
        auto record_it = m_entity_records.find(id);
        if (record_it == m_entity_records.end()) {
            return; // already destroyed or never existed
        }

        Entity* entity = record_it->second.ptr;
        const bool was_pending = record_it->second.live_index == INVALID_ENTITY_INDEX;

        // Destroy the whole subtree. Snapshot the children because each recursive call mutates this list.
        std::vector<Entity*> children;
        entity->get_child_entities(children);
        for (const Entity* child : children) {
            destroy_entity(child->get_id());
        }

        if (was_pending) {
            // Pending entities never enter the world, so exit_world()/detach won't run. Detach synchronously to
            // avoid leaving a dangling pointer in a surviving parent (or in a still-in-play child).
            if (TransformComponent* transform = entity->get_transform()) {
                transform->detach_from_hierarchy();
            }
            else if (TransformComponent3D* transform_3d = entity->get_transform_3d()) {
                transform_3d->detach_from_hierarchy();
            }

            // Drop the pending spawn request, which frees the Entity's unique_ptr.
            std::erase_if(m_entity_spawn_requests, [&](const std::unique_ptr<Entity>& e) {
                return e->get_id() == id;
            });

            // The record's raw Entity* now dangles, so erase it too.
            m_entity_records.erase(id);
            return;
        }

        // Mark for destroy if already spawned. Live subtree members detach in exit_world() during resolve.
        m_entity_destroy_requests.insert(id);
    }

    Entity* EntitySpawner::get_entity(EntityId id) const {
        auto it = m_entity_records.find(id);
        if (it != m_entity_records.end()) {
            return it->second.ptr;
        }

        return nullptr;
    }

    void EntitySpawner::get_entities(std::vector<Entity*>& out_entities) const {
        out_entities.clear();
        out_entities.reserve(m_entities.size());
        for (const auto& entity : m_entities) {
            out_entities.push_back(entity.get());
        }
    }

    void EntitySpawner::register_ticking_entity(Entity* entity) {
        entity->m_tick_index = static_cast<TickIndex>(m_ticking_entities.size());
        m_ticking_entities.push_back(entity);
    }

    void EntitySpawner::unregister_ticking_entity(Entity* entity) {
        // Swap-pop; fix the moved entity's stored index.
        const TickIndex index = entity->m_tick_index;
        HOB_ASSERT(index != INVALID_TICK_INDEX && index < m_ticking_entities.size(),
                   "Unregistering an entity that isn't registered for ticking");
        const TickIndex last_index = static_cast<TickIndex>(m_ticking_entities.size() - 1);
        if (index != last_index) {
            m_ticking_entities[index] = m_ticking_entities[last_index];
            m_ticking_entities[index]->m_tick_index = index;
        }

        m_ticking_entities.pop_back();
        entity->m_tick_index = INVALID_TICK_INDEX;
    }

    void EntitySpawner::request_entity_ticking_sync(EntityId id) {
        m_entity_ticking_sync_requests.insert(id);
    }

    const std::vector<Entity*>& EntitySpawner::get_ticking_entities() const {
        return m_ticking_entities;
    }

    void EntitySpawner::register_mesh_renderer(MeshRendererComponent* mesh_renderer) {
        mesh_renderer->m_mesh_renderer_index = static_cast<MeshRendererIndex>(m_mesh_renderers.size());
        m_mesh_renderers.push_back(mesh_renderer);
    }

    void EntitySpawner::unregister_mesh_renderer(MeshRendererComponent* mesh_renderer) {
        const MeshRendererIndex index = mesh_renderer->m_mesh_renderer_index;
        HOB_ASSERT(index != INVALID_MESH_RENDERER_INDEX && index < m_mesh_renderers.size(),
                   "Unregistering a mesh renderer that isn't registered");
        const MeshRendererIndex last_index = static_cast<MeshRendererIndex>(m_mesh_renderers.size() - 1);
        if (index != last_index) {
            m_mesh_renderers[index] = m_mesh_renderers[last_index];
            m_mesh_renderers[index]->m_mesh_renderer_index = index;
        }

        m_mesh_renderers.pop_back();
        mesh_renderer->m_mesh_renderer_index = INVALID_MESH_RENDERER_INDEX;
    }

    const std::vector<MeshRendererComponent*>& EntitySpawner::get_mesh_renderers() const {
        return m_mesh_renderers;
    }

    void EntitySpawner::register_sprite(SpriteComponent* sprite) {
        sprite->m_sprite_index = static_cast<SpriteIndex>(m_sprites.size());
        m_sprites.push_back(sprite);
    }

    void EntitySpawner::unregister_sprite(SpriteComponent* sprite) {
        // Swap-pop; fix the moved sprite's stored index.
        const SpriteIndex index = sprite->m_sprite_index;
        HOB_ASSERT(index != INVALID_SPRITE_INDEX && index < m_sprites.size(),
                   "Unregistering a sprite that isn't registered");
        const SpriteIndex last_index = static_cast<SpriteIndex>(m_sprites.size() - 1);
        if (index != last_index) {
            m_sprites[index] = m_sprites[last_index];
            m_sprites[index]->m_sprite_index = index;
        }

        m_sprites.pop_back();
        sprite->m_sprite_index = INVALID_SPRITE_INDEX;
    }

    const std::vector<SpriteComponent*>& EntitySpawner::get_sprites() const {
        return m_sprites;
    }

    void EntitySpawner::register_simulated_rigidbody(RigidbodyComponent* rigidbody) {
        rigidbody->m_rigidbody_index = static_cast<RigidbodyIndex>(m_simulated_rigidbodies.size());
        m_simulated_rigidbodies.push_back(rigidbody);
    }

    void EntitySpawner::unregister_simulated_rigidbody(RigidbodyComponent* rigidbody) {
        // Swap-pop; fix the moved rigidbody's stored index.
        const RigidbodyIndex index = rigidbody->m_rigidbody_index;
        HOB_ASSERT(index != INVALID_RIGIDBODY_INDEX && index < m_simulated_rigidbodies.size(),
                   "Unregistering a rigidbody that isn't registered");
        const RigidbodyIndex last_index = static_cast<RigidbodyIndex>(m_simulated_rigidbodies.size() - 1);
        if (index != last_index) {
            m_simulated_rigidbodies[index] = m_simulated_rigidbodies[last_index];
            m_simulated_rigidbodies[index]->m_rigidbody_index = index;
        }

        m_simulated_rigidbodies.pop_back();
        rigidbody->m_rigidbody_index = INVALID_RIGIDBODY_INDEX;
    }

    const std::vector<RigidbodyComponent*>& EntitySpawner::get_simulated_rigidbodies() const {
        return m_simulated_rigidbodies;
    }

    void EntitySpawner::register_simulated_rigidbody_3d(RigidbodyComponent3D* rigidbody) {
        rigidbody->m_rigidbody_index = static_cast<RigidbodyIndex3D>(m_simulated_rigidbodies_3d.size());
        m_simulated_rigidbodies_3d.push_back(rigidbody);
    }

    void EntitySpawner::unregister_simulated_rigidbody_3d(RigidbodyComponent3D* rigidbody) {
        const RigidbodyIndex3D index = rigidbody->m_rigidbody_index;
        HOB_ASSERT(index != INVALID_RIGIDBODY_INDEX_3D && index < m_simulated_rigidbodies_3d.size(),
                   "Unregistering a 3D rigidbody that isn't registered");
        const RigidbodyIndex3D last_index = static_cast<RigidbodyIndex3D>(m_simulated_rigidbodies_3d.size() - 1);
        if (index != last_index) {
            m_simulated_rigidbodies_3d[index] = m_simulated_rigidbodies_3d[last_index];
            m_simulated_rigidbodies_3d[index]->m_rigidbody_index = index;
        }

        m_simulated_rigidbodies_3d.pop_back();
        rigidbody->m_rigidbody_index = INVALID_RIGIDBODY_INDEX_3D;
    }

    const std::vector<RigidbodyComponent3D*>& EntitySpawner::get_simulated_rigidbodies_3d() const {
        return m_simulated_rigidbodies_3d;
    }

    void EntitySpawner::register_audio(AudioComponent* audio) {
        audio->m_audio_index = static_cast<AudioIndex>(m_audio_sources.size());
        m_audio_sources.push_back(audio);
    }

    void EntitySpawner::unregister_audio(AudioComponent* audio) {
        // Swap-pop; fix the moved source's stored index.
        const AudioIndex index = audio->m_audio_index;
        HOB_ASSERT(index != INVALID_AUDIO_INDEX && index < m_audio_sources.size(),
                   "Unregistering an audio source that isn't registered");
        const AudioIndex last_index = static_cast<AudioIndex>(m_audio_sources.size() - 1);
        if (index != last_index) {
            m_audio_sources[index] = m_audio_sources[last_index];
            m_audio_sources[index]->m_audio_index = index;
        }

        m_audio_sources.pop_back();
        audio->m_audio_index = INVALID_AUDIO_INDEX;
    }

    const std::vector<AudioComponent*>& EntitySpawner::get_audio_sources() const {
        return m_audio_sources;
    }

    void EntitySpawner::clear() {
        for (auto& entity : m_entities) {
            entity->exit_play();
            entity->exit_world();
        }

        m_entities.clear();
        m_entity_records.clear();
        m_entity_spawn_requests.clear();
        m_entity_destroy_requests.clear();
        m_entity_ticking_sync_requests.clear();
        m_entity_spawn_requests_swap_buffer.clear();
        m_entity_destroy_requests_swap_buffer.clear();
        m_entity_ticking_sync_requests_swap_buffer.clear();

        if (m_entities_cleared_handler) {
            m_entities_cleared_handler();
        }
    }

    void EntitySpawner::register_cvars(Console& console) {
        console.register_cvar("e_show_hierarchy",
                              "Show an entity hierarchy window (nested transforms, id, components)",
                              to_cvar_string(m_cvar_show_hierarchy),
                              ConsoleVariableType::Bool,
                              ConsoleVariableFlags::None,
                              [this](const ConsoleVariable& cvar) {
                                  m_cvar_show_hierarchy = cvar.bool_value();
                              });
    }

    void EntitySpawner::debug_hierarchy() {
        if (!m_cvar_show_hierarchy) {
            return;
        }

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowSize(ImVec2(700.0f, 550.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Entity Hierarchy", nullptr, window_flags)) {
            ImGui::BeginChild("Tree", ImVec2(300.0f, 0.0f), ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
            {
                ImGui::Text("Entities: %zu", m_entities.size());
                ImGui::Separator();

                for (const auto& entity : m_entities) {
                    if (!entity) {
                        continue;
                    }

                    if (entity->get_parent_entity() != nullptr) {
                        continue; // Reached via its parent instead.
                    }

                    debug_hierarchy_node(*entity);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Inspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
            debug_inspector();
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void EntitySpawner::debug_hierarchy_node(const Entity& entity) {
        std::vector<Entity*> children;
        entity.get_child_entities(children);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (entity.get_id() == m_selected_entity_id) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity.get_id())),
                                            flags,
                                            "%s  #%lld",
                                            entity.get_display_name().c_str(),
                                            static_cast<long long>(entity.get_id()));

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_selected_entity_id = entity.get_id();
        }

        if (open) {
            for (const Entity* child : children) {
                debug_hierarchy_node(*child);
            }
            ImGui::TreePop();
        }
    }

    void EntitySpawner::debug_inspector() {
        Entity* entity = get_entity(m_selected_entity_id);
        if (entity == nullptr) {
            ImGui::TextDisabled("Select an entity");
            return;
        }

        ImGui::Text("%s", entity->get_display_name().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("#%lld", static_cast<long long>(entity->get_id()));

        const char* prefab_name = !entity->get_prefab_name().empty() ? entity->get_prefab_name().c_str() : "";
        ImGui::TextDisabled("Prefab: %s", prefab_name);

        constexpr float label_width = 100.0f;
        constexpr ImGuiInputTextFlags field_flags = ImGuiInputTextFlags_ReadOnly;

        const auto bool_field = [&](const char* label, const char* id, bool value) {
            char buffer[8];
            std::strcpy(buffer, value ? "true" : "false");
            ImGui::TextUnformatted(label);
            ImGui::SameLine(label_width);
            ImGui::SetNextItemWidth(-EPSILON);
            ImGui::InputText(id, buffer, sizeof(buffer), field_flags);
        };

        bool_field("Ticking", "##ticking", entity->is_ticking());

        if (const TransformComponent* transform = entity->get_transform()) {
            ImGui::SeparatorText("Transform");

            const auto vec2_field = [&](const char* label, const char* id, const Vector2& v) {
                float xy[2] = {v.x, v.y};
                ImGui::TextUnformatted(label);
                ImGui::SameLine(label_width);
                ImGui::SetNextItemWidth(-EPSILON);
                ImGui::InputFloat2(id, xy, "%.2f", field_flags);
            };

            const Vector2 local_pos = transform->get_local_position();
            float local_rot = transform->get_local_rotation();
            const Vector2 local_scale = transform->get_local_scale();

            vec2_field("Position", "##position", local_pos);

            ImGui::TextUnformatted("Rotation");
            ImGui::SameLine(label_width);
            ImGui::SetNextItemWidth(-EPSILON);
            ImGui::InputFloat("##rotation", &local_rot, 0.0f, 0.0f, "%.2f", field_flags);

            vec2_field("Scale", "##scale", local_scale);
        }
        else if (const TransformComponent3D* transform_3d = entity->get_transform_3d()) {
            ImGui::SeparatorText("Transform 3D");

            const auto vec3_field = [&](const char* label, const char* id, const Vector3& v) {
                float xyz[3] = {v.x, v.y, v.z};
                ImGui::TextUnformatted(label);
                ImGui::SameLine(label_width);
                ImGui::SetNextItemWidth(-EPSILON);
                ImGui::InputFloat3(id, xyz, "%.2f", field_flags);
            };

            vec3_field("Position", "##position", transform_3d->get_local_position());
            vec3_field("Rotation", "##rotation", transform_3d->get_local_euler_deg());
            vec3_field("Scale", "##scale", transform_3d->get_local_scale());
        }

        ImGui::SeparatorText("Components");
        entity->for_each_component<Component>([](const Component* component) {
            ImGui::BulletText("%s", component->to_string().c_str());
        });
    }

    void EntitySpawner::resolve_requests() {
        resolve_spawn_requests();
        resolve_destroy_requests();
        resolve_ticking_sync_requests();
    }

    void EntitySpawner::resolve_spawn_requests() {
        // Swap because someone might make a spawn request in enter_play()
        m_entity_spawn_requests_swap_buffer.swap(m_entity_spawn_requests);

        for (auto& entity : m_entity_spawn_requests_swap_buffer) {
            const EntityId entity_id = entity->get_id();
            m_entity_records[entity_id].live_index = static_cast<EntityIndex>(m_entities.size());
            m_entities.emplace_back(std::move(entity));
            m_entities.back()->enter_world();

            if (m_engine.is_in_play()) {
                m_entities.back()->enter_play();
            }

            if (m_entity_spawned_handler) {
                m_entity_spawned_handler(entity_id);
            }
        }

        m_entity_spawn_requests_swap_buffer.clear();
    }

    void EntitySpawner::resolve_destroy_requests() {
        // Swap because someone might make a destroy request in exit_play()
        m_entity_destroy_requests_swap_buffer.swap(m_entity_destroy_requests);

        // exit_play() while entities are still present in the map
        for (const EntityId id : m_entity_destroy_requests_swap_buffer) {
            Entity* entity = get_entity(id);
            if (entity != nullptr) {
                entity->exit_play();
                entity->exit_world();

                if (m_entity_destroyed_handler) {
                    m_entity_destroyed_handler(id);
                }
            }
        }

        for (const EntityId id : m_entity_destroy_requests_swap_buffer) {
            auto it = m_entity_records.find(id);
            if (it == m_entity_records.end()) {
                continue;
            }

            // Swap-pop; fix the moved entity's stored index in its record.
            const EntityIndex index = it->second.live_index;
            HOB_ASSERT(index != INVALID_ENTITY_INDEX && index < m_entities.size(),
                       "Queued destroy request must be an in-play entity");
            const EntityIndex last_index = static_cast<EntityIndex>(m_entities.size() - 1);
            if (index != last_index) {
                m_entities[index] = std::move(m_entities[last_index]);
                m_entity_records[m_entities[index]->get_id()].live_index = index;
            }

            m_entities.pop_back();
            m_entity_records.erase(it);
        }

        m_entity_destroy_requests_swap_buffer.clear();
    }

    void EntitySpawner::resolve_ticking_sync_requests() {
        // Swap because someone might request a ticking change in a sync below;
        // (register/unregister doesn't call back here, but stay consistent with the spawn/destroy resolve pattern).
        m_entity_ticking_sync_requests_swap_buffer.swap(m_entity_ticking_sync_requests);

        for (const EntityId id : m_entity_ticking_sync_requests_swap_buffer) {
            Entity* entity = get_entity(id);
            if (entity == nullptr) {
                continue; // Destroyed before the sync; exit_play() already unregistered it.
            }

            // Sync membership to the requested state (idempotent: a no-op if they already match).
            const bool request = entity->m_is_ticking_request;
            if (request && !entity->is_ticking()) {
                register_ticking_entity(entity);
            }
            else if (!request && entity->is_ticking()) {
                unregister_ticking_entity(entity);
            }
        }

        m_entity_ticking_sync_requests_swap_buffer.clear();
    }
} // namespace hob
