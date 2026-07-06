#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/systems/audio/audio.h"
#include "lua_meta.h"
#include "lua_schema_factory.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        AudioClipRef resolve_clip(Audio& audio, const sol::object& value) {
            if (value.is<AudioClipRef>()) {
                return value.as<AudioClipRef>();
            }
            if (value.is<std::string>()) {
                return audio.get_or_load_clip(value.as<std::string>());
            }
            return nullptr;
        }
    } // namespace

    void LuaScriptSystem::bind_audio() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        LuaFactorySchemaRegistry& factory_schemas = m_impl->factory_schemas;
        Audio& audio = m_engine.get_audio();

        bind_usertype<AudioClip>(lua, meta)
            .factory_ctor(
                [&audio](const sol::table& cfg) -> AudioClipRef {
                    const std::string path = cfg.get<sol::optional<std::string>>("path").value_or("");
                    if (path.empty()) {
                        log::lua.error("DefineAudioClip requires a 'path'");
                        return nullptr;
                    }
                    return audio.get_or_load_clip(path);
                },
                {"config"})
            .method("get_path", &AudioClip::get_path);

        bind_factory_schema<AudioClip>(factory_schemas, "DefineAudioClip", "AudioClips", {"path"});

        bind_table(lua, meta, "Audio")
            .func_sig(
                "play_oneshot",
                [&audio](const sol::object& clip, sol::optional<float> volume) {
                    AudioClipRef clip_ref = resolve_clip(audio, clip);
                    audio.play_oneshot(clip_ref, volume.value_or(1.0f));
                },
                "(clip: AudioClip, volume: number?)")
            .func("get_master_volume",
                  [&audio]() {
                      return audio.get_master_volume();
                  })
            .func("set_master_volume",
                  [&audio](float volume) {
                      audio.set_master_volume(volume);
                  },
                  {"volume"});
    }
} // namespace hob
