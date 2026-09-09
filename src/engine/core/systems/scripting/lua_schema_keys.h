#pragma once

namespace hob {
    // Definition files are Lua source under an extension that names what they declare. The suffix is
    // what tells the editor which files it owns, and `.meta` is the companion of an imported binary
    // rather than an asset of its own, so it keeps the full original filename: "player.png.meta".
    namespace file_extension {
        constexpr const char* LUA = ".lua";
        constexpr const char* SCENE = ".scene";
        constexpr const char* PREFAB = ".prefab";
        constexpr const char* MATERIAL = ".mat";
        constexpr const char* ANIMATION_CLIP = ".anim";
        constexpr const char* SHADER = ".shader";
        constexpr const char* META = ".meta";
    } // namespace file_extension

    namespace def_registry {
        constexpr const char* SCENES = "Scenes";
        constexpr const char* ENTITIES = "Entities";
        constexpr const char* COMPONENTS = "Components";
        constexpr const char* TEXTURES = "Textures";
        constexpr const char* SHADERS = "Shaders";
        constexpr const char* MATERIALS = "Materials";
        constexpr const char* ANIMATION_CLIPS = "AnimationClips";
        constexpr const char* AUDIO_CLIPS = "AudioClips";
    } // namespace def_registry

    namespace component_schema_key {
        // Component schema section.
        constexpr const char* ADD = "add";
        constexpr const char* GET = "get";
        constexpr const char* GETTERS = "getters";
        constexpr const char* SETTERS = "setters";
        constexpr const char* ORDER = "__order";
        constexpr const char* TYPES = "types";
        constexpr const char* REAPPLY_ON_HOT_RELOAD = "reapply_on_hot_reload";

        // Per-field metadata.
        constexpr const char* TYPE = "type";
        constexpr const char* ENUM = "enum";
        constexpr const char* MIN = "min";
        constexpr const char* MAX = "max";
        constexpr const char* HIDDEN = "hidden";
    } // namespace component_schema_key

    namespace scene_key {
        constexpr const char* POSE_OVERRIDES = "pose_overrides";
        constexpr const char* CPP_OVERRIDES = "cpp_overrides";
        constexpr const char* LUA_OVERRIDES = "lua_overrides";
    } // namespace scene_key

    namespace prefab_key {
        constexpr const char* TICKING = "ticking";
        constexpr const char* LUA_COMPONENTS = "lua_components";
        constexpr const char* LUA_FIELDS = "lua_fields";
    } // namespace prefab_key

    namespace transform_key {
        constexpr const char* SECTION = "transform";
        constexpr const char* POSITION = "position";
        constexpr const char* ROTATION = "rotation";
        constexpr const char* SCALE = "scale";

        // The scene document spells the transform's rotation in degrees, under its own key.
        constexpr const char* ROTATION_DEG = "rotation_deg";
    } // namespace transform_key

    namespace field_type {
        constexpr const char* INT = "int";
        constexpr const char* FLOAT = "float";
        constexpr const char* BOOL = "bool";
        constexpr const char* STRING = "string";
        constexpr const char* VECTOR2 = "vector2";
        constexpr const char* COLOR = "color";
        constexpr const char* ANGLE = "angle"; // Stored in radians
        constexpr const char* ENUM = "enum";
        constexpr const char* BITMASK = "bitmask";
        constexpr const char* AABB = "aabb";
        constexpr const char* CAPSULE = "capsule";
        constexpr const char* CIRCLE = "circle";
        constexpr const char* TEXTURE = "texture";
        constexpr const char* MATERIAL = "material";
        constexpr const char* ANIMATION_CLIP = "animation_clip";
        constexpr const char* AUDIO_CLIP = "audio_clip";

        constexpr const char* OTHER = "other";
    } // namespace field_type
} // namespace hob
