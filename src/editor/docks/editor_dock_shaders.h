#pragma once

#include "editor_dock.h"

namespace hob::editor {
    class EditorDockShaders : public EditorDock {
    public:
        EditorDockShaders();

        void draw(Editor& editor) override;
    };
} // namespace hob::editor
