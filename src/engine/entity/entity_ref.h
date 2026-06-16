#pragma once

#include "engine/entity/entity.h"

namespace hob {
    class EntitySpawner;

    class EntityRef {
        EntityId m_id = INVALID_ENTITY_ID;
        EntitySpawner* m_spawner = nullptr;

    public:
        EntityRef() = default;
        EntityRef(EntityId id, EntitySpawner& spawner);

        EntityId get_id() const;
        Entity* resolve() const;
        bool is_valid() const;
    };
} // namespace hob
