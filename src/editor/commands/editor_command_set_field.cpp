#include "editor_command_set_field.h"

#include <string>
#include <utility>

#include "editor/editor.h"
#include "editor/editor_lua.h"
#include "engine/components/physics/rigidbody_component.h"
#include "engine/components/physics_3d/rigidbody_component_3d.h"
#include "engine/components/transform_component.h"
#include "engine/components/transform_component_3d.h"
#include "engine/core/engine.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"

namespace hob::editor {
    namespace {
        void sync_transform_to_physics(Engine& engine, EntityId entity_id) {
            Entity* entity = engine.get_entity_spawner().get_entity(entity_id);
            if (entity == nullptr) {
                return;
            }

            if (const TransformComponent3D* transform_3d = entity->get_transform_3d()) {
                RigidbodyComponent3D* rigidbody_3d = entity->get_rigidbody_3d();
                if (rigidbody_3d != nullptr && rigidbody_3d->get_body_type() != BodyType::Static) {
                    rigidbody_3d->set_position(transform_3d->get_position());
                    rigidbody_3d->set_rotation(transform_3d->get_rotation());
                }
                return;
            }

            RigidbodyComponent* rigidbody = entity->get_rigidbody();
            if (rigidbody == nullptr || rigidbody->get_body_type() == BodyType::Static) {
                return;
            }

            const TransformComponent* transform = entity->get_transform();
            rigidbody->set_position(transform->get_position());
            rigidbody->set_rotation(transform->get_rotation());
        }

        EntityId resolve_entity_id(Engine& engine, const EditorFieldTarget& target) {
            if (target.instance_id == INVALID_EDITOR_INSTANCE_ID) {
                return target.entity_id;
            }

            return get_entity_id_of_instance(engine, target.instance_id);
        }
    } // namespace

    EditorCommandSetField::EditorCommandSetField(std::string label,
                                                 EditorFieldTarget target,
                                                 sol::object old_value,
                                                 sol::object new_value)
        : EditorCommand(std::move(label))
        , m_target(std::move(target))
        , m_old_value(std::move(old_value))
        , m_new_value(std::move(new_value)) {}

    void EditorCommandSetField::undo(Editor& editor) {
        apply(editor, m_target, m_old_value);
    }

    void EditorCommandSetField::redo(Editor& editor) {
        apply(editor, m_target, m_new_value);
    }

    void EditorCommandSetField::apply(Editor& editor, const EditorFieldTarget& target, const sol::object& value) {
        Engine& engine = editor.get_engine();

        if (target.is_prefab_document()) {
            if (editor.get_state() != WorldState::Stopped) {
                return;
            }

            const char* set_prefab_field =
                target.is_lua ? editor_func::SET_PREFAB_LUA_FIELD : editor_func::SET_PREFAB_FIELD;

            editor_call(engine, set_prefab_field, target.prefab_name, target.component_key, target.field, value);
            return;
        }

        const EntityId entity_id = resolve_entity_id(engine, target);
        if (entity_id == INVALID_ENTITY_ID) {
            return;
        }

        const char* set_component_field =
            target.is_lua ? editor_func::SET_LUA_COMPONENT_FIELD : editor_func::SET_COMPONENT_FIELD;

        const sol::object set_successful =
            editor_call(engine, set_component_field, entity_id, target.component_key, target.field, value);
        if (!set_successful.is<bool>() || !set_successful.as<bool>()) {
            return;
        }

        if (editor.get_state() == WorldState::Stopped) {
            const char* set_instance_field =
                target.is_lua ? editor_func::SET_LUA_INSTANCE_FIELD : editor_func::SET_INSTANCE_FIELD;

            editor_call(engine, set_instance_field, entity_id, target.component_key, target.field, value);
        }

        if (!target.is_lua &&
            (target.component_key == transform_key::SECTION || target.component_key == transform_3d_key::SECTION)) {
            sync_transform_to_physics(engine, entity_id);
        }
    }
} // namespace hob::editor
