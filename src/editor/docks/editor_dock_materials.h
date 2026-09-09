#pragma once

#include "editor_dock.h"

namespace hob::editor {
    class EditorDockMaterials : public EditorDock {
    public:
        EditorDockMaterials();

        void draw(Editor& editor) override;
    };
} // namespace hob::editor
