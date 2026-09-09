#pragma once

#include "editor_dock.h"

namespace hob::editor {
    class EditorDockSpriteQueue : public EditorDock {
    public:
        EditorDockSpriteQueue();

        void draw(Editor& editor) override;
    };
} // namespace hob::editor
