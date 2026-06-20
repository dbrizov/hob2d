#include "engine/core/engine.h"
#include "engine/core/systems/input.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_input() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        Input& input = m_engine.get_input();

        bind_table(lua, meta, "Input")
            .func("get_mouse_screen_position",
                  [&input]() {
                      return input.get_mouse_screen_position();
                  })
            .func("is_gamepad_connected", [&input]() {
                return input.is_gamepad_connected();
            });
    }
} // namespace hob
