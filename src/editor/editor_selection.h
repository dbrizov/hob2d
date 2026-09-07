#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "editor_definition.h"
#include "editor_instance_id.h"
#include "engine/entity/entity.h"

namespace hob::editor {
    struct EditorSelectionClick {
        EntityId entity_id = INVALID_ENTITY_ID;
        bool additive = false; // Ctrl
        bool range = false; // Shift
    };

    // The Inspector shows one thing, so entities and a definition document are mutually exclusive:
    // selecting either clears the other, which is why they share a struct rather than a panel each.
    struct EditorSelection {
        std::vector<EntityId> ids;
        EntityId range_anchor = INVALID_ENTITY_ID;
        EditorDefinitionRef definition;

        bool empty() const {
            return ids.empty() && !definition.is_valid();
        }

        bool contains(EntityId id) const {
            return std::ranges::find(ids, id) != ids.end();
        }

        EntityId primary() const {
            return ids.empty() ? INVALID_ENTITY_ID : ids.back();
        }

        void clear() {
            ids.clear();
            range_anchor = INVALID_ENTITY_ID;
            definition = EditorDefinitionRef{};
        }

        void set(EntityId id) {
            ids.clear();
            definition = EditorDefinitionRef{};
            ids.push_back(id);
        }

        void add(EntityId id) {
            remove(id);
            definition = EditorDefinitionRef{};
            ids.push_back(id);
        }

        void select_definition(EditorDefinitionRef selected) {
            ids.clear();
            range_anchor = INVALID_ENTITY_ID;
            definition = std::move(selected);
        }

        void remove(EntityId id) {
            std::erase(ids, id);
        }

        void toggle(EntityId id) {
            if (contains(id)) {
                remove(id);
            }
            else {
                add(id);
            }
        }

        // visible_order is the rows a range select may span, and is empty where ranges make no sense.
        void apply_click(const EditorSelectionClick& click, const std::vector<EntityId>& visible_order);
    };

    struct EditorSelectionInstanceIds {
        std::vector<EditorInstanceId> ids;
        EditorInstanceId range_anchor = INVALID_EDITOR_INSTANCE_ID;
    };
} // namespace hob::editor
