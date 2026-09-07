#include "editor_prefabs.h"

#include <format>
#include <memory>

#include "commands/editor_command_add_prefab_section.h"
#include "commands/editor_command_apply_to_prefab.h"
#include "commands/editor_command_remove_prefab_section.h"
#include "commands/editor_command_revert_override.h"
#include "editor.h"
#include "editor_gui_utils.h"
#include "editor_lua.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"

namespace hob::editor {
    namespace {
        std::string to_section_label(const std::string& key, bool is_lua) {
            const std::string label = to_display_label(key);
            return is_lua ? label + " (Lua)" : label;
        }

        std::string get_entity_prefab_name(Engine& engine, EntityId entity_id) {
            const Entity* entity = engine.get_entity_spawner().get_entity(entity_id);
            return entity != nullptr ? entity->get_prefab_name() : std::string();
        }

        EditorFieldTarget to_prefab_target(const EditorFieldTarget& target, const std::string& prefab_name) {
            EditorFieldTarget prefab_target = target;
            prefab_target.entity_id = INVALID_ENTITY_ID;
            prefab_target.instance_id = INVALID_EDITOR_INSTANCE_ID;
            prefab_target.prefab_name = prefab_name;

            return prefab_target;
        }

        bool is_nil(const sol::object& value) {
            return !value.valid() || value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
        }

        sol::object get_instance_override(Engine& engine, const EditorFieldTarget& target) {
            return editor_call(engine,
                               editor_func::GET_INSTANCE_OVERRIDE,
                               target.entity_id,
                               target.component_key,
                               target.field,
                               target.is_lua);
        }
    } // namespace

    bool can_edit_prefab_document(const Editor& editor, const EditorDefinitionRef& ref) {
        if (ref.registry != def_registry::ENTITIES || editor.get_state() != WorldState::Stopped) {
            return false;
        }

        const EditorDefinition* definition = find_definition(editor.get_engine(), ref);
        return definition != nullptr && !definition->read_only;
    }

    std::vector<EditorPrefabSectionEntry> get_addable_prefab_sections(Engine& engine, const std::string& prefab_name) {
        std::vector<EditorPrefabSectionEntry> entries;

        const sol::object result = editor_call(engine, editor_func::GET_ADDABLE_PREFAB_SECTIONS, prefab_name);
        if (!result.is<sol::table>()) {
            return entries;
        }

        const sol::table rows = result.as<sol::table>();
        entries.reserve(rows.size());

        for (int32_t i = 1; i <= static_cast<int32_t>(rows.size()); ++i) {
            const sol::object row = rows[i];
            if (!row.is<sol::table>()) {
                continue;
            }

            const sol::table entry = row.as<sol::table>();
            entries.push_back({.name = entry.get_or<std::string>(query_key::NAME, ""),
                               .is_lua = entry.get_or(query_key::IS_LUA, false)});
        }

        return entries;
    }

    bool add_prefab_section(Editor& editor,
                            const std::string& prefab_name,
                            const std::string& key,
                            bool is_lua,
                            const sol::object& removed) {
        Engine& engine = editor.get_engine();

        const sol::object added =
            removed.valid() ? editor_call(engine, editor_func::ADD_PREFAB_SECTION, prefab_name, key, is_lua, removed)
                            : editor_call(engine, editor_func::ADD_PREFAB_SECTION, prefab_name, key, is_lua);
        if (!added.is<bool>() || !added.as<bool>()) {
            return false;
        }

        editor.respawn_prefab_instances(prefab_name);

        return true;
    }

    sol::object remove_prefab_section(Editor& editor,
                                      const std::string& prefab_name,
                                      const std::string& key,
                                      bool is_lua) {
        const sol::object removed =
            editor_call(editor.get_engine(), editor_func::REMOVE_PREFAB_SECTION, prefab_name, key, is_lua);
        if (!removed.is<sol::table>()) {
            return sol::object{};
        }

        editor.respawn_prefab_instances(prefab_name);

        return removed;
    }

    void request_add_prefab_section(Editor& editor,
                                    const std::string& prefab_name,
                                    const std::string& key,
                                    bool is_lua) {
        editor.get_commands().push(editor,
                                   std::make_unique<EditorCommandAddPrefabSection>(
                                       std::format("Add {}", to_section_label(key, is_lua)), prefab_name, key, is_lua));
    }

    bool can_apply_to_prefab(const Editor& editor, const EditorFieldTarget& target) {
        if (editor.get_state() != WorldState::Stopped || target.instance_id == INVALID_EDITOR_INSTANCE_ID) {
            return false;
        }

        const std::string prefab_name = get_entity_prefab_name(editor.get_engine(), target.entity_id);
        return !prefab_name.empty() &&
               can_edit_prefab_document(editor, {.registry = def_registry::ENTITIES, .name = prefab_name});
    }

    void request_apply_to_prefab(Editor& editor, const EditorFieldTarget& target) {
        if (!can_apply_to_prefab(editor, target)) {
            return;
        }

        Engine& engine = editor.get_engine();

        const sol::object value = get_instance_override(engine, target);
        if (is_nil(value)) {
            return;
        }

        const EditorFieldTarget prefab_target =
            to_prefab_target(target, get_entity_prefab_name(engine, target.entity_id));
        const sol::object previous = editor_call(engine,
                                                 editor_func::GET_PREFAB_FIELD,
                                                 prefab_target.prefab_name,
                                                 prefab_target.component_key,
                                                 prefab_target.field,
                                                 prefab_target.is_lua);

        editor.get_commands().push(editor,
                                   std::make_unique<EditorCommandApplyToPrefab>(
                                       std::format("Apply {} to Prefab", to_display_label(target.field)),
                                       target,
                                       prefab_target,
                                       value,
                                       previous));
    }

    void request_revert_override(Editor& editor, const EditorFieldTarget& target) {
        if (editor.get_state() != WorldState::Stopped || target.instance_id == INVALID_EDITOR_INSTANCE_ID) {
            return;
        }

        const sol::object value = get_instance_override(editor.get_engine(), target);
        if (is_nil(value)) {
            return;
        }

        editor.get_commands().push(
            editor,
            std::make_unique<EditorCommandRevertOverride>(
                std::format("Revert {} to Prefab", to_display_label(target.field)), target, value));
    }

    void request_remove_prefab_section(Editor& editor,
                                       const std::string& prefab_name,
                                       const std::string& key,
                                       bool is_lua) {
        editor.get_commands().push(
            editor,
            std::make_unique<EditorCommandRemovePrefabSection>(
                std::format("Remove {}", to_section_label(key, is_lua)), prefab_name, key, is_lua));
    }
} // namespace hob::editor
