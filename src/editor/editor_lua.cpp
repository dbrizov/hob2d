#include "editor_lua.h"

#include <unordered_map>

#include "editor_style.h"
#include "engine/core/asset.h"
#include "engine/core/engine.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"
#include "engine/core/systems/scripting/lua_script_system.h"

namespace hob::editor {
    namespace {
        constexpr const char* INSPECTOR_UNNAMED_LABEL = "(unnamed)";

        std::unordered_map<std::string, std::vector<EditorInspectorEntryAsset>> g_asset_entries;
        std::unordered_map<std::string, TextureRef> g_textures;
        std::vector<EditorDefinition> g_definitions;
        bool g_definitions_cached = false;
    } // namespace

    sol::protected_function get_editor_func(Engine& engine, const char* name) {
        const sol::object editor_table = engine.get_lua_script_system().get_lua()["Editor"];
        if (!editor_table.is<sol::table>()) {
            return sol::protected_function{};
        }

        return editor_table.as<sol::table>()[name];
    }

    EditorInstanceId get_instance_id_of_entity(Engine& engine, EntityId entity_id) {
        const sol::object result = editor_call(engine, editor_func::GET_INSTANCE_ID, entity_id);
        return result.is<EditorInstanceId>() ? result.as<EditorInstanceId>() : INVALID_EDITOR_INSTANCE_ID;
    }

    EntityId get_entity_id_of_instance(Engine& engine, EditorInstanceId instance_id) {
        const sol::object result = editor_call(engine, editor_func::GET_ENTITY_ID, instance_id);
        return result.is<EntityId>() ? result.as<EntityId>() : INVALID_ENTITY_ID;
    }

    bool is_asset_set(const sol::object& value) {
        if (!value.valid()) {
            return false;
        }

        if (value.get_type() == sol::type::userdata) {
            return value.is<Asset>() && value.as<Asset*>() != nullptr;
        }

        return true;
    }

    std::string get_asset_name(Engine& engine, const std::string& factory_name, const sol::object& object) {
        if (!is_asset_set(object)) {
            return {};
        }

        const sol::object result = editor_call(engine, editor_func::GET_ASSET_NAME, factory_name, object);
        return result.is<std::string>() ? result.as<std::string>() : std::string();
    }

    const char* get_asset_factory_name_for_field_type(std::string_view type) {
        if (type == field_type::TEXTURE) {
            return def_registry::TEXTURES;
        }

        if (type == field_type::MATERIAL) {
            return def_registry::MATERIALS;
        }

        if (type == field_type::ANIMATION_CLIP) {
            return def_registry::ANIMATION_CLIPS;
        }

        if (type == field_type::AUDIO_CLIP) {
            return def_registry::AUDIO_CLIPS;
        }

        if (type == field_type::MESH) {
            return def_registry::MESHES;
        }

        return nullptr;
    }

    void clear_lua_query_caches() {
        g_asset_entries.clear();
        g_textures.clear();
        g_definitions.clear();
        g_definitions_cached = false;
    }

    const std::vector<EditorInspectorEntryAsset>& get_asset_entries(Engine& engine, const std::string& factory_name) {
        const auto cached = g_asset_entries.find(factory_name);
        if (cached != g_asset_entries.end()) {
            return cached->second;
        }

        const sol::object result = editor_call(engine, editor_func::GET_ASSET_ENTRIES, factory_name);
        if (!result.is<sol::table>()) {
            static const std::vector<EditorInspectorEntryAsset> empty;
            return empty;
        }

        const sol::table rows = result.as<sol::table>();

        std::vector<EditorInspectorEntryAsset> entries;
        entries.reserve(rows.size());

        for (int32_t i = 1; i <= static_cast<int32_t>(rows.size()); ++i) {
            const sol::object row = rows[i];
            if (!row.is<sol::table>()) {
                continue;
            }

            const sol::table entry = row.as<sol::table>();
            entries.push_back({.name = entry.get_or<std::string>(query_key::NAME, "")});
        }

        return g_asset_entries.emplace(factory_name, std::move(entries)).first->second;
    }

    const std::vector<EditorDefinition>& get_definitions(Engine& engine) {
        if (g_definitions_cached) {
            return g_definitions;
        }

        const sol::object result = editor_call(engine, editor_func::GET_DEFINITIONS);
        if (!result.is<sol::table>()) {
            return g_definitions;
        }

        const sol::table rows = result.as<sol::table>();
        g_definitions.reserve(rows.size());

        for (int32_t i = 1; i <= static_cast<int32_t>(rows.size()); ++i) {
            const sol::object row = rows[i];
            if (!row.is<sol::table>()) {
                continue;
            }

            const sol::table entry = row.as<sol::table>();
            g_definitions.push_back({.ref = {.registry = entry.get_or<std::string>(query_key::REGISTRY, ""),
                                             .name = entry.get_or<std::string>(query_key::NAME, "")},
                                     .file = entry.get_or<std::string>(query_key::FILE, ""),
                                     .read_only = entry.get_or(query_key::READ_ONLY, true)});
        }

        g_definitions_cached = true;

        return g_definitions;
    }

    const EditorDefinition* find_definition(Engine& engine, const EditorDefinitionRef& ref) {
        for (const EditorDefinition& definition : get_definitions(engine)) {
            if (definition.ref == ref) {
                return &definition;
            }
        }

        return nullptr;
    }

    TextureRef get_texture(Engine& engine, const std::string& asset_name) {
        const auto cached = g_textures.find(asset_name);
        if (cached != g_textures.end()) {
            return cached->second;
        }

        const sol::object result = editor_call(engine, editor_func::BUILD_ASSET, def_registry::TEXTURES, asset_name);

        TextureRef texture;
        if (result.is<Texture>()) {
            texture = result.as<TextureRef>();
        }

        return g_textures.emplace(asset_name, std::move(texture)).first->second;
    }

    std::vector<EditorInspectorEntryEnum> get_enum_entries(Engine& engine, const std::string& name) {
        std::vector<EditorInspectorEntryEnum> entries;

        const sol::object result = editor_call(engine, editor_func::GET_ENUM_ENTRIES, name);
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
            entries.emplace_back(entry.get_or<std::string>(query_key::NAME, ""),
                                 entry.get_or<int64_t>(query_key::VALUE, 0));
        }

        return entries;
    }

    std::string lua_object_to_display_string(Engine& engine, const sol::object& value) {
        if (!value.valid()) {
            return INSPECTOR_NONE_LABEL;
        }

        if (value.get_type() == sol::type::userdata && value.is<Asset>()) {
            const Asset* asset = value.as<Asset*>();
            if (asset == nullptr) {
                return INSPECTOR_NONE_LABEL;
            }

            return !asset->get_name().empty() ? asset->get_name() : INSPECTOR_UNNAMED_LABEL;
        }

        const sol::protected_function to_string = engine.get_lua_script_system().get_lua()["tostring"];
        if (to_string.valid()) {
            const sol::protected_function_result result = to_string(value);
            if (result.valid()) {
                return result.get<std::string>();
            }
        }

        return "?";
    }
} // namespace hob::editor
