#include "editor_command_revert_override.h"

#include <utility>

#include "editor/editor.h"
#include "editor/editor_lua.h"
#include "editor_command_set_field.h"
#include "engine/core/engine.h"

namespace hob::editor {
    EditorCommandRevertOverride::EditorCommandRevertOverride(std::string label,
                                                             EditorFieldTarget target,
                                                             sol::object override)
        : EditorCommand(std::move(label))
        , m_target(std::move(target))
        , m_override(std::move(override)) {}

    void EditorCommandRevertOverride::undo(Editor& editor) {
        EditorCommandSetField::apply(editor, m_target, m_override);
    }

    void EditorCommandRevertOverride::redo(Editor& editor) {
        Engine& engine = editor.get_engine();

        const EntityId entity_id = get_entity_id_of_instance(engine, m_target.instance_id);
        if (entity_id == INVALID_ENTITY_ID) {
            return;
        }

        editor_call(engine,
                    editor_func::CLEAR_INSTANCE_FIELD,
                    entity_id,
                    m_target.component_key,
                    m_target.field,
                    m_target.is_lua);
    }
} // namespace hob::editor
