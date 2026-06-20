#pragma once

#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/entity/entity.h"

namespace hob {
    class Engine;
    class SpriteComponent;
    class RigidbodyComponent;

    using EntityIndex = uint32_t;
    constexpr EntityIndex INVALID_ENTITY_INDEX = std::numeric_limits<EntityIndex>::max();

    // Per-id bookkeeping.
    // - 'ptr' is valid from spawn_entity() until the entity exits play.
    // - 'live_index' is the slot in m_entities once the entity enters play.
    //   While the entity is still in m_entity_spawn_requests 'live_index' stays INVALID_ENTITY_INDEX.
    struct EntityRecord {
        Entity* ptr = nullptr;
        EntityIndex live_index = INVALID_ENTITY_INDEX;
    };

    class EntitySpawner {
        Engine& m_engine;

        EntityId m_next_entity_id = 0;
        std::vector<std::unique_ptr<Entity>> m_entities;
        std::unordered_map<EntityId, EntityRecord> m_entity_records;

        std::vector<std::unique_ptr<Entity>> m_entity_spawn_requests;
        std::unordered_set<EntityId> m_entity_destroy_requests;
        std::unordered_set<EntityId> m_entity_ticking_sync_requests;

        std::vector<Entity*> m_ticking_entities; // Registry of in-play entities with ticking enabled
        std::vector<SpriteComponent*> m_sprites; // Registry of in-play sprites
        std::vector<RigidbodyComponent*> m_simulated_rigidbodies; // Registry of in-play non-static rigidbodies

    public:
        explicit EntitySpawner(Engine& engine);
        ~EntitySpawner();

        Entity& spawn_entity();
        void destroy_entity(EntityId id);

        Entity* get_entity(EntityId id) const;
        void get_entities(std::vector<Entity*>& out_entities) const;

        void register_ticking_entity(Entity* entity);
        void unregister_ticking_entity(Entity* entity);
        void request_entity_ticking_sync(EntityId id);
        const std::vector<Entity*>& get_ticking_entities() const;

        void register_sprite(SpriteComponent* sprite);
        void unregister_sprite(SpriteComponent* sprite);
        const std::vector<SpriteComponent*>& get_sprites() const;

        void register_simulated_rigidbody(RigidbodyComponent* rigidbody);
        void unregister_simulated_rigidbody(RigidbodyComponent* rigidbody);
        const std::vector<RigidbodyComponent*>& get_simulated_rigidbodies() const;

    private:
        friend class Engine;
        void resolve_requests();
        void resolve_spawn_requests();
        void resolve_destroy_requests();
        void resolve_ticking_sync_requests();

        void clear();
    };
} // namespace hob
