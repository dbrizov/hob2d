#include "engine/core/engine.h"
#include "engine/core/systems/timer.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_timer() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        Timer& timer = m_engine.get_timer();

        bind_table(lua, meta, "Timer")
            .func("get_fps",
                  [&timer]() {
                      return timer.get_fps();
                  })
            .func("set_fps",
                  [&timer](uint32_t v) {
                      timer.set_fps(v);
                  },
                  {"fps"})
            .func("get_time_scale",
                  [&timer]() {
                      return timer.get_time_scale();
                  })
            .func("set_time_scale",
                  [&timer](float v) {
                      timer.set_time_scale(v);
                  },
                  {"scale"})
            .func("get_delta_time",
                  [&timer]() {
                      return timer.get_delta_time();
                  })
            .func("get_game_time",
                  [&timer]() {
                      return timer.get_game_time();
                  })
            .func("get_real_time", [&timer]() {
                return timer.get_real_time();
            });
    }
} // namespace hob
