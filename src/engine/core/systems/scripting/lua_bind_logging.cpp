#include <string>

#include "engine/core/logging.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    void LuaScriptSystem::bind_logging() {
        sol::state& m_lua = m_impl->lua;
        LuaMetaRegistry& m_meta = m_impl->meta;

        auto stringify_args = [](sol::this_state ts, sol::variadic_args args) -> std::string {
            sol::state_view sv(ts);
            const sol::protected_function tostring = sv["tostring"];
            std::string out;
            bool first = true;
            for (auto v : args) {
                sol::protected_function_result r = tostring(sol::object(v));
                const std::string piece = r.valid() ? r.get<std::string>() : "<tostring failed>";
                if (!first) {
                    out += '\t';
                }
                out += piece;
                first = false;
            }

            return out;
        };

        bind_table(m_lua, m_meta, "Log")
            .func_sig(
                "info",
                [stringify_args](sol::this_state ts, sol::variadic_args args) {
                    log::info("{}", stringify_args(ts, args));
                },
                "(...: any)")
            .func_sig(
                "error",
                [stringify_args](sol::this_state ts, sol::variadic_args args) {
                    log::error("{}", stringify_args(ts, args));
                },
                "(...: any)");
    }
} // namespace hob
