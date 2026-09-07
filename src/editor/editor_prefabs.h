#pragma once

#include <string>
#include <vector>

#include <sol/sol.hpp>

#include "editor_definition.h"

namespace hob {
    class Engine;
} // namespace hob

namespace hob::editor {
    class Editor;

    struct EditorPrefabSectionEntry {
        std::string name; // Schema key, or the Lua class name when is_lua
        bool is_lua = false;
    };

    bool can_edit_prefab_document(const Editor& editor, const EditorDefinitionRef& ref);

    std::vector<EditorPrefabSectionEntry> get_addable_prefab_sections(Engine& engine, const std::string& prefab_name);

    bool add_prefab_section(Editor& editor,
                            const std::string& prefab_name,
                            const std::string& key,
                            bool is_lua,
                            const sol::object& removed);
    sol::object remove_prefab_section(Editor& editor,
                                      const std::string& prefab_name,
                                      const std::string& key,
                                      bool is_lua);

    void request_add_prefab_section(Editor& editor,
                                    const std::string& prefab_name,
                                    const std::string& key,
                                    bool is_lua);
    void request_remove_prefab_section(Editor& editor,
                                       const std::string& prefab_name,
                                       const std::string& key,
                                       bool is_lua);
} // namespace hob::editor
