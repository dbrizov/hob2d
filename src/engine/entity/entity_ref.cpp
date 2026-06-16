#include "entity_ref.h"

#include "engine/core/systems/entity_spawner.h"

namespace hob {
    EntityRef::EntityRef(EntityId id, EntitySpawner& spawner)
        : m_id(id)
        , m_spawner(&spawner) {}

    EntityId EntityRef::get_id() const {
        return m_id;
    }

    Entity* EntityRef::resolve() const {
        return m_spawner ? m_spawner->get_entity(m_id) : nullptr;
    }

    bool EntityRef::is_valid() const {
        return resolve() != nullptr;
    }
} // namespace hob
