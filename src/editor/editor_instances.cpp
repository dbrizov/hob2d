#include "editor_instances.h"

#include <format>
#include <memory>
#include <utility>
#include <vector>

#include "commands/editor_command_add_instance.h"
#include "commands/editor_command_composite.h"
#include "commands/editor_command_remove_instance.h"
#include "editor.h"
#include "editor_lua.h"
#include "engine/core/engine.h"

namespace hob::editor {
    namespace {
        constexpr const char* DUPLICATE_LABEL = "Duplicate";
        constexpr const char* DELETE_LABEL = "Delete";

        std::unique_ptr<EditorCommand> to_command(const char* label,
                                                  std::vector<std::unique_ptr<EditorCommand>> commands) {
            if (commands.size() == 1) {
                return std::move(commands.front());
            }

            return std::make_unique<EditorCommandComposite>(label, std::move(commands));
        }

        std::vector<EditorInstanceId> get_selected_instance_ids(const Editor& editor) {
            std::vector<EditorInstanceId> instance_ids;

            for (EntityId entity_id : editor.get_selection().ids) {
                const EditorInstanceId instance_id = get_instance_id_of_entity(editor.get_engine(), entity_id);
                if (instance_id != INVALID_EDITOR_INSTANCE_ID) {
                    instance_ids.push_back(instance_id);
                }
            }

            return instance_ids;
        }
    } // namespace

    EditorInstanceId add_instance(Editor& editor, const sol::table& instance, int32_t index) {
        Engine& engine = editor.get_engine();

        const sol::object result = (index == APPEND_INSTANCE_INDEX)
                                       ? editor_call(engine, editor_func::ADD_INSTANCE, instance)
                                       : editor_call(engine, editor_func::ADD_INSTANCE, instance, index);
        if (!result.is<EditorInstanceId>()) {
            return INVALID_EDITOR_INSTANCE_ID;
        }

        const EditorInstanceId instance_id = result.as<EditorInstanceId>();

        const EntityId entity_id = get_entity_id_of_instance(engine, instance_id);
        if (entity_id != INVALID_ENTITY_ID) {
            editor.get_selection().add(entity_id);
        }

        return instance_id;
    }

    int32_t remove_instance(Editor& editor, EditorInstanceId instance_id) {
        const EntityId entity_id = get_entity_id_of_instance(editor.get_engine(), instance_id);

        const sol::object result = editor_call(editor.get_engine(), editor_func::REMOVE_INSTANCE, instance_id);

        editor.get_selection().remove(entity_id);

        return result.is<int32_t>() ? result.as<int32_t>() : APPEND_INSTANCE_INDEX;
    }

    bool can_edit_scene_instances(const Editor& editor) {
        return editor.get_state() == WorldState::Stopped && !editor.get_current_scene().empty();
    }

    bool can_edit_selected_instances(const Editor& editor) {
        return can_edit_scene_instances(editor) && !editor.get_selection().ids.empty();
    }

    void add_prefab_instance(Editor& editor, const std::string& prefab_name, const Vector2& position) {
        if (!can_edit_scene_instances(editor)) {
            return;
        }

        const sol::object instance =
            editor_call(editor.get_engine(), editor_func::CREATE_INSTANCE_DEF, prefab_name, position);
        if (!instance.is<sol::table>()) {
            return;
        }

        editor.get_selection().clear();
        editor.get_commands().push(
            editor,
            std::make_unique<EditorCommandAddInstance>(std::format("Add {}", prefab_name), instance.as<sol::table>()));
    }

    void duplicate_selection(Editor& editor) {
        if (!can_edit_selected_instances(editor)) {
            return;
        }

        Engine& engine = editor.get_engine();

        std::vector<std::unique_ptr<EditorCommand>> commands;
        for (EditorInstanceId instance_id : get_selected_instance_ids(editor)) {
            const sol::object instance = editor_call(engine, editor_func::COPY_INSTANCE_DEF, instance_id);
            if (instance.is<sol::table>()) {
                commands.push_back(
                    std::make_unique<EditorCommandAddInstance>(DUPLICATE_LABEL, instance.as<sol::table>()));
            }
        }

        if (commands.empty()) {
            return;
        }

        editor.get_selection().clear();
        editor.get_commands().push(editor, to_command(DUPLICATE_LABEL, std::move(commands)));
    }

    void delete_selection(Editor& editor) {
        if (!can_edit_selected_instances(editor)) {
            return;
        }

        Engine& engine = editor.get_engine();

        std::vector<std::unique_ptr<EditorCommand>> commands;
        for (EditorInstanceId instance_id : get_selected_instance_ids(editor)) {
            const sol::object instance = editor_call(engine, editor_func::GET_INSTANCE_DEF, instance_id);
            if (instance.is<sol::table>()) {
                commands.push_back(std::make_unique<EditorCommandRemoveInstance>(
                    DELETE_LABEL, instance.as<sol::table>(), instance_id));
            }
        }

        if (commands.empty()) {
            return;
        }

        editor.get_commands().push(editor, to_command(DELETE_LABEL, std::move(commands)));
    }
} // namespace hob::editor
