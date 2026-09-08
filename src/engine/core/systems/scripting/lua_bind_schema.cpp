#include "lua_meta.h"
#include "lua_schema_keys.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_schema() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;

        bind_table(lua, meta, "FileExtension")
            .constant("LUA", file_extension::LUA)
            .constant("SCENE", file_extension::SCENE)
            .constant("PREFAB", file_extension::PREFAB)
            .constant("MATERIAL", file_extension::MATERIAL)
            .constant("ANIMATION_CLIP", file_extension::ANIMATION_CLIP)
            .constant("SHADER", file_extension::SHADER)
            .constant("META", file_extension::META)
            .constant("MESH", file_extension::MESH);

        bind_table(lua, meta, "DefRegistry")
            .constant("SCENES", def_registry::SCENES)
            .constant("ENTITIES", def_registry::ENTITIES)
            .constant("COMPONENTS", def_registry::COMPONENTS)
            .constant("TEXTURES", def_registry::TEXTURES)
            .constant("SHADERS", def_registry::SHADERS)
            .constant("MATERIALS", def_registry::MATERIALS)
            .constant("ANIMATION_CLIPS", def_registry::ANIMATION_CLIPS)
            .constant("AUDIO_CLIPS", def_registry::AUDIO_CLIPS)
            .constant("MESHES", def_registry::MESHES);

        bind_table(lua, meta, "SceneKey")
            .constant("POSE_OVERRIDES", scene_key::POSE_OVERRIDES)
            .constant("CPP_OVERRIDES", scene_key::CPP_OVERRIDES)
            .constant("LUA_OVERRIDES", scene_key::LUA_OVERRIDES);

        bind_table(lua, meta, "PrefabKey")
            .constant("TICKING", prefab_key::TICKING)
            .constant("LUA_COMPONENTS", prefab_key::LUA_COMPONENTS)
            .constant("LUA_FIELDS", prefab_key::LUA_FIELDS);

        bind_table(lua, meta, "SpaceKey")
            .constant("KEY", space_key::KEY)
            .constant("SPACE_2D", space_key::SPACE_2D)
            .constant("SPACE_3D", space_key::SPACE_3D);

        bind_table(lua, meta, "TransformKey3D")
            .constant("SECTION", transform_3d_key::SECTION)
            .constant("POSITION", transform_key::POSITION)
            .constant("ROTATION", transform_key::ROTATION)
            .constant("ROTATION_DEG", transform_key::ROTATION_DEG)
            .constant("SCALE", transform_key::SCALE);

        bind_table(lua, meta, "TransformKey")
            .constant("SECTION", transform_key::SECTION)
            .constant("POSITION", transform_key::POSITION)
            .constant("ROTATION", transform_key::ROTATION)
            .constant("ROTATION_DEG", transform_key::ROTATION_DEG)
            .constant("SCALE", transform_key::SCALE);

        bind_table(lua, meta, "FieldType")
            .constant("INT", field_type::INT)
            .constant("FLOAT", field_type::FLOAT)
            .constant("BOOL", field_type::BOOL)
            .constant("STRING", field_type::STRING)
            .constant("VECTOR2", field_type::VECTOR2)
            .constant("VECTOR3", field_type::VECTOR3)
            .constant("QUATERNION", field_type::QUATERNION)
            .constant("EULER_DEG", field_type::EULER_DEG)
            .constant("COLOR", field_type::COLOR)
            .constant("ANGLE", field_type::ANGLE)
            .constant("ENUM", field_type::ENUM)
            .constant("BITMASK", field_type::BITMASK)
            .constant("TEXTURE", field_type::TEXTURE)
            .constant("MATERIAL", field_type::MATERIAL)
            .constant("ANIMATION_CLIP", field_type::ANIMATION_CLIP)
            .constant("AUDIO_CLIP", field_type::AUDIO_CLIP)
            .constant("MESH", field_type::MESH)
            .constant("AABB", field_type::AABB)
            .constant("AABB3", field_type::AABB3)
            .constant("CAPSULE", field_type::CAPSULE)
            .constant("CIRCLE", field_type::CIRCLE)
            .constant("OTHER", field_type::OTHER);
    }
} // namespace hob
