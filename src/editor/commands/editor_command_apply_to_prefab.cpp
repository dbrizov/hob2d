#include "editor_command_apply_to_prefab.h"

#include <utility>

#include "editor/editor.h"
#include "editor/editor_lua.h"
#include "editor_command_set_field.h"
#include "engine/core/engine.h"

namespace hob::editor {
    EditorCommandApplyToPrefab::EditorCommandApplyToPrefab(std::string label,
                                                           EditorFieldTarget instance_target,
                                                           EditorFieldTarget prefab_target,
                                                           sol::object value,
                                                           sol::object previous_prefab_value)
        : EditorCommand(std::move(label))
        , m_instance_target(std::move(instance_target))
        , m_prefab_target(std::move(prefab_target))
        , m_value(std::move(value))
        , m_previous_prefab_value(std::move(previous_prefab_value)) {}

    void EditorCommandApplyToPrefab::undo(Editor& editor) {
        Engine& engine = editor.get_engine();

        EditorCommandSetField::apply(editor, m_instance_target, m_value);

        if (m_previous_prefab_value.valid() && m_previous_prefab_value != sol::lua_nil) {
            EditorCommandSetField::apply(editor, m_prefab_target, m_previous_prefab_value);
        }
        else {
            editor_call(engine,
                        editor_func::REMOVE_PREFAB_FIELD,
                        m_prefab_target.prefab_name,
                        m_prefab_target.component_key,
                        m_prefab_target.field,
                        m_prefab_target.is_lua);
        }
    }

    void EditorCommandApplyToPrefab::redo(Editor& editor) {
        Engine& engine = editor.get_engine();

        EditorCommandSetField::apply(editor, m_prefab_target, m_value);

        const EntityId entity_id = get_entity_id_of_instance(engine, m_instance_target.instance_id);
        if (entity_id != INVALID_ENTITY_ID) {
            editor_call(engine,
                        editor_func::CLEAR_INSTANCE_FIELD,
                        entity_id,
                        m_instance_target.component_key,
                        m_instance_target.field,
                        m_instance_target.is_lua);
        }
    }
} // namespace hob::editor
