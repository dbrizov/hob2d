#pragma once

#include "editor_dock.h"

namespace hob::editor {
    class EditorDockTextures : public EditorDock {
    public:
        EditorDockTextures();

        void draw(Editor& editor) override;
    };
} // namespace hob::editor
