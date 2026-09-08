#pragma once

#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engine/core/space.h"
#include "engine/entity/entity.h"

namespace hob {
    class Engine;
    class Console;
    class AudioComponent;
    class MeshRendererComponent;
    class SpriteComponent;
    class TransformComponent;
    class RigidbodyComponent;
    class RigidbodyComponent3D;

    using EntityIndex = uint32_t;
    constexpr EntityIndex INVALID_ENTITY_INDEX = std::numeric_limits<EntityIndex>::max();

    template<typename Func>
    concept EntityInvocable = std::invocable<Func, Entity*>;

    template<typename Pred>
    concept EntityPredicate = std::predicate<Pred, Entity*>;

    struct EntityRecord {
        Entity* ptr = nullptr;
        EntityIndex live_index = INVALID_ENTITY_INDEX;
    };

    class EntitySpawner {
        friend class Engine;

        Engine& m_engine;

        std::function<void(EntityId)> m_entity_spawned_handler;
        std::function<void(EntityId)> m_entity_destroyed_handler;
        std::function<void()> m_entities_cleared_handler;

        EntityId m_next_entity_id = 0;
        std::vector<std::unique_ptr<Entity>> m_entities;
        std::unordered_map<EntityId, EntityRecord> m_entity_records;

        std::vector<std::unique_ptr<Entity>> m_entity_spawn_requests;
        std::unordered_set<EntityId> m_entity_destroy_requests;
        std::unordered_set<EntityId> m_entity_ticking_sync_requests;

        std::vector<std::unique_ptr<Entity>> m_entity_spawn_requests_swap_buffer;
        std::unordered_set<EntityId> m_entity_destroy_requests_swap_buffer;
        std::unordered_set<EntityId> m_entity_ticking_sync_requests_swap_buffer;

        std::vector<Entity*> m_ticking_entities; // Registry of in-play entities with ticking enabled
        std::vector<SpriteComponent*> m_sprites; // Registry of in-play sprites
        std::vector<MeshRendererComponent*> m_mesh_renderers; // Registry of in-world mesh renderers
        std::vector<RigidbodyComponent*> m_simulated_rigidbodies; // Registry of in-play non-static rigidbodies
        std::vector<RigidbodyComponent3D*> m_simulated_rigidbodies_3d; // Registry of in-world non-static 3D rigidbodies
        std::vector<AudioComponent*> m_audio_sources; // Registry of in-play audio sources

        bool m_cvar_show_hierarchy = false;
        EntityId m_selected_entity_id = INVALID_ENTITY_ID; // Entity shown in the inspector pane.

    public:
        explicit EntitySpawner(Engine& engine);
        ~EntitySpawner();

        EntitySpawner(const EntitySpawner&) = delete;
        EntitySpawner& operator=(const EntitySpawner&) = delete;

        EntitySpawner(EntitySpawner&&) = delete;
        EntitySpawner& operator=(EntitySpawner&&) = delete;

        Entity& spawn_entity(Space space = Space::Space2D);
        void destroy_entity(EntityId id);

        void set_entity_spawned_handler(std::function<void(EntityId)> callback);
        void set_entity_destroyed_handler(std::function<void(EntityId)> callback);
        void set_entities_cleared_handler(std::function<void()> callback);

        Entity* get_entity(EntityId id) const;
        void get_entities(std::vector<Entity*>& out_entities) const;

        template<EntityInvocable Func>
        void for_each_entity(Func&& func) const;

        template<EntityInvocable Func, EntityPredicate Until>
        void for_each_entity(Func&& func, Until&& until) const;

        void register_ticking_entity(Entity* entity);
        void unregister_ticking_entity(Entity* entity);
        void request_entity_ticking_sync(EntityId id);
        const std::vector<Entity*>& get_ticking_entities() const;

        void register_sprite(SpriteComponent* sprite);
        void unregister_sprite(SpriteComponent* sprite);
        const std::vector<SpriteComponent*>& get_sprites() const;

        void register_mesh_renderer(MeshRendererComponent* mesh_renderer);
        void unregister_mesh_renderer(MeshRendererComponent* mesh_renderer);
        const std::vector<MeshRendererComponent*>& get_mesh_renderers() const;

        void register_simulated_rigidbody(RigidbodyComponent* rigidbody);
        void unregister_simulated_rigidbody(RigidbodyComponent* rigidbody);
        const std::vector<RigidbodyComponent*>& get_simulated_rigidbodies() const;

        void register_simulated_rigidbody_3d(RigidbodyComponent3D* rigidbody);
        void unregister_simulated_rigidbody_3d(RigidbodyComponent3D* rigidbody);
        const std::vector<RigidbodyComponent3D*>& get_simulated_rigidbodies_3d() const;

        void register_audio(AudioComponent* audio);
        void unregister_audio(AudioComponent* audio);
        const std::vector<AudioComponent*>& get_audio_sources() const;

        void clear();

        void register_cvars(Console& console);
        void debug_hierarchy();

    private:
        void debug_hierarchy_node(const Entity& entity);
        void debug_inspector();

        void resolve_requests();
        void resolve_spawn_requests();
        void resolve_destroy_requests();
        void resolve_ticking_sync_requests();
    };

    template<EntityInvocable Func>
    void EntitySpawner::for_each_entity(Func&& func) const {
        for_each_entity(std::forward<Func>(func), [](Entity*) {
            return false;
        });
    }

    template<EntityInvocable Func, EntityPredicate Until>
    void EntitySpawner::for_each_entity(Func&& func, Until&& until) const {
        for (const auto& entity : m_entities) {
            func(entity.get());
            if (until(entity.get())) {
                return;
            }
        }
    }
} // namespace hob
