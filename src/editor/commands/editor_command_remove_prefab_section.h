#pragma once

#include <string>

#include <sol/sol.hpp>

#include "editor_command.h"

namespace hob::editor {
    class EditorCommandRemovePrefabSection : public EditorCommand {
        std::string m_prefab_name;
        std::string m_key;
        bool m_is_lua;
        sol::object m_removed;

    public:
        EditorCommandRemovePrefabSection(std::string label, std::string prefab_name, std::string key, bool is_lua);

        void undo(Editor& editor) override;
        void redo(Editor& editor) override;
    };
} // namespace hob::editor
