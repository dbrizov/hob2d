#include <string>

#include "engine/core/logging.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        int32_t lua_panic_handler(lua_State* L) {
            const char* message = lua_tostring(L, -1);
            log::sol2.error("panic: {}", message != nullptr ? message : "unknown error");
            return 0;
        }

        void lua_warn_handler(void* ud, const char* message, int32_t tocont) {
            (void)ud;
            static std::string buffer;
            buffer += message;
            if (!tocont) {
                log::sol2.error("{}", buffer);
                buffer.clear();
            }
        }
    } // namespace

    void LuaScriptSystem::install_log_redirects() {
        sol::state& lua = m_impl->lua;
        lua_State* L = lua.lua_state();
        lua_atpanic(L, lua_panic_handler);
        lua_setwarnf(L, lua_warn_handler, nullptr);

        lua["print"] = [](sol::this_state ts, sol::variadic_args args) {
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

            log::sol2.info("{}", out);
        };
    }

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
