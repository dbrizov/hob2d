#pragma once

#include <string>

#include "engine/entity/entity.h"

namespace hob::editor {
    struct EditorFieldTarget {
        EntityId entity_id = INVALID_ENTITY_ID;
        bool is_lua = false;
        std::string component_key; // Schema key, e.g. "sprite", or the Lua class name when is_lua
        std::string field;
        std::string prefab_name; // Set when the target is a prefab document rather than a live entity

        bool operator==(const EditorFieldTarget& other) const = default;

        bool has_same_owner(const EditorFieldTarget& other) const {
            return entity_id == other.entity_id && prefab_name == other.prefab_name;
        }

        bool is_prefab_document() const {
            return !prefab_name.empty();
        }
    };
} // namespace hob::editor
