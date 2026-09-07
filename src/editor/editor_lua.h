#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sol/sol.hpp>

#include "editor_definition.h"
#include "editor_inspector_entries.h"
#include "editor_instance_id.h"
#include "engine/core/logging.h"
#include "engine/core/systems/renderer/texture.h"
#include "engine/entity/entity.h"

namespace hob {
    class Engine;
} // namespace hob

namespace hob::editor {
    namespace query_key {
        constexpr const char* NAME = "name";
        constexpr const char* VALUE = "value";
        constexpr const char* TYPE = "type";
        constexpr const char* IS_LUA = "is_lua";
        constexpr const char* FIELDS = "fields";
        constexpr const char* REGISTRY = "registry";
        constexpr const char* FILE = "file";
        constexpr const char* READ_ONLY = "read_only";
    } // namespace query_key

    namespace editor_func {
        constexpr const char* GET_SCENE_NAMES = "get_scene_names";
        constexpr const char* IS_SCENE_DIRTY = "is_scene_dirty";
        constexpr const char* MARK_SCENE_SAVED = "mark_scene_saved";
        constexpr const char* GET_SCENE_FILE = "get_scene_file";
        constexpr const char* GET_SCENE_SAVE_ERROR = "get_scene_save_error";
        constexpr const char* GET_SCENE_NAME_FOR_FILE = "get_scene_name_for_file";
        constexpr const char* GET_SCENE_CREATE_ERROR = "get_scene_create_error";
        constexpr const char* SERIALIZE_SCENE = "serialize_scene";
        constexpr const char* SERIALIZE_NEW_SCENE = "serialize_new_scene";
        constexpr const char* OPEN_SCENE = "open_scene";
        constexpr const char* LOAD_SCENE = "load_scene";
        constexpr const char* CLEAR_WORLD = "clear_world";
        constexpr const char* GET_ENTITY_ID = "get_entity_id";
        constexpr const char* GET_INSTANCE_ID = "get_instance_id";
        constexpr const char* GET_INSTANCE_DEF = "get_instance_def";
        constexpr const char* CREATE_INSTANCE_DEF = "create_instance_def";
        constexpr const char* COPY_INSTANCE_DEF = "copy_instance_def";
        constexpr const char* ADD_INSTANCE = "add_instance";
        constexpr const char* REMOVE_INSTANCE = "remove_instance";
        constexpr const char* REBIND_INSTANCE_DEFS = "rebind_instance_defs";
        constexpr const char* SET_INSTANCE_FIELD = "set_instance_field";
        constexpr const char* SET_LUA_INSTANCE_FIELD = "set_lua_instance_field";

        constexpr const char* GET_COMPONENTS = "get_components";
        constexpr const char* GET_ENUM_ENTRIES = "get_enum_entries";
        constexpr const char* GET_ASSET_ENTRIES = "get_asset_entries";
        constexpr const char* GET_ASSET_NAME = "get_asset_name";
        constexpr const char* GET_ASSET_REF = "get_asset_ref";
        constexpr const char* GET_DEFINITIONS = "get_definitions";
        constexpr const char* GET_DEFINITION_SECTIONS = "get_definition_sections";
        constexpr const char* BUILD_ASSET = "build_asset";

        constexpr const char* SET_COMPONENT_FIELD = "set_component_field";
        constexpr const char* SET_LUA_COMPONENT_FIELD = "set_lua_component_field";

        constexpr const char* SET_PREFAB_FIELD = "set_prefab_field";
        constexpr const char* SET_PREFAB_LUA_FIELD = "set_prefab_lua_field";
        constexpr const char* IS_PREFAB_DIRTY = "is_prefab_dirty";
        constexpr const char* GET_DIRTY_PREFAB_NAMES = "get_dirty_prefab_names";
        constexpr const char* REBIND_PREFAB_DEFS = "rebind_prefab_defs";
        constexpr const char* MARK_PREFAB_SAVED = "mark_prefab_saved";
        constexpr const char* MARK_PREFAB_REVERTED = "mark_prefab_reverted";
        constexpr const char* GET_PREFAB_FILE = "get_prefab_file";
        constexpr const char* GET_PREFAB_SAVE_ERROR = "get_prefab_save_error";
        constexpr const char* SERIALIZE_PREFAB = "serialize_prefab";
        constexpr const char* SERIALIZE_NEW_PREFAB = "serialize_new_prefab";
    } // namespace editor_func

    sol::protected_function get_editor_func(Engine& engine, const char* name);

    template<typename... Args>
    sol::object editor_call(Engine& engine, const char* name, Args&&... args) {
        const sol::protected_function func = get_editor_func(engine, name);
        if (!func.valid()) {
            return sol::object{};
        }

        sol::protected_function_result result = func(std::forward<Args>(args)...);
        if (!result.valid()) {
            const sol::error error = result;
            log::editor.error("Editor.{}: {}", name, error.what());

            return sol::object{};
        }

        return result;
    }

    EditorInstanceId get_instance_id_of_entity(Engine& engine, EntityId entity_id);
    EntityId get_entity_id_of_instance(Engine& engine, EditorInstanceId instance_id);

    bool is_asset_set(const sol::object& value);

    std::string get_asset_name(Engine& engine, const std::string& factory_name, const sol::object& object);
    const char* get_asset_factory_name_for_field_type(std::string_view type);

    void clear_lua_query_caches();
    const std::vector<EditorInspectorEntryAsset>& get_asset_entries(Engine& engine, const std::string& factory_name);
    const std::vector<EditorDefinition>& get_definitions(Engine& engine);
    const EditorDefinition* find_definition(Engine& engine, const EditorDefinitionRef& ref);
    TextureRef get_texture(Engine& engine, const std::string& asset_name);
    std::vector<EditorInspectorEntryEnum> get_enum_entries(Engine& engine, const std::string& name);

    std::string lua_object_to_display_string(Engine& engine, const sol::object& value);
} // namespace hob::editor
