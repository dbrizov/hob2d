#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "editor_dock.h"
#include "engine/entity/entity.h"

namespace hob::editor {
    struct EditorFileTree;

    struct EditorEntityDrop {
        EntityId entity_id = INVALID_ENTITY_ID;
        std::filesystem::path folder;
    };

    class EditorDockAssets : public EditorDock {
        std::unique_ptr<EditorFileTree> m_tree;
        std::optional<EditorEntityDrop> m_pending_drop;

    public:
        EditorDockAssets();
        ~EditorDockAssets() override;

        void draw(Editor& editor) override;

        void poll(Editor& editor);

        void request_rebuild();
    };
} // namespace hob::editor
