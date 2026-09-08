#pragma once

#include <vector>

#include "editor_dock.h"
#include "engine/entity/entity.h"

namespace hob {
    class Entity;
} // namespace hob

namespace hob::editor {
    class EditorDockHierarchy : public EditorDock {
        bool m_scroll_to_primary = false;

    public:
        EditorDockHierarchy();

        void draw(Editor& editor) override;

        void scroll_to_primary();

    private:
        void draw_entity(const Editor& editor,
                         const Entity& entity,
                         std::vector<EntityId>& visible_order,
                         EntityId& out_clicked_entity_id);
    };
} // namespace hob::editor
