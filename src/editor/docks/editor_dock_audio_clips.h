#pragma once

#include "editor_dock.h"

namespace hob::editor {
    class EditorDockAudioClips : public EditorDock {
    public:
        EditorDockAudioClips();

        void draw(Editor& editor) override;
    };
} // namespace hob::editor
