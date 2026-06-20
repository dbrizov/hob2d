#include <string>
#include <unordered_map>
#include <variant>

#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/systems/ui/ui_system.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
    namespace {
        UiValue lua_to_ui_value(const sol::object& obj) {
            switch (obj.get_type()) {
                case sol::type::boolean:
                    return obj.as<bool>();
                case sol::type::number:
                    return obj.is<int64_t>() ? UiValue{obj.as<int64_t>()} : UiValue{obj.as<double>()};
                case sol::type::string:
                    return obj.as<std::string>();
                default:
                    return UiValue{};
            }
        }

        sol::object ui_value_to_lua(sol::state_view lua, const UiValue& value) {
            return std::visit(
                [&lua](const auto& v) {
                    return sol::make_object(lua, v);
                },
                value);
        }
    } // namespace

    void LuaScriptSystem::bind_ui() {
        sol::state& lua = m_impl->lua;
        LuaMetaRegistry& meta = m_impl->meta;
        UiSystem& ui = m_engine.get_ui_system();

        bind_table(lua, meta, "UI")
            .func("load_document",
                  [&ui](const std::string& path) {
                      return ui.load_document(path);
                  },
                  {"path"})
            .func("unload_document",
                  [&ui](UiDocumentId id) {
                      ui.unload_document(id);
                  },
                  {"document"})
            .func("show_document",
                  [&ui](UiDocumentId id) {
                      ui.show_document(id);
                  },
                  {"document"})
            .func("hide_document",
                  [&ui](UiDocumentId id) {
                      ui.hide_document(id);
                  },
                  {"document"})
            .func("get_element",
                  [&ui](UiDocumentId document_id, const std::string& element_id) {
                      return ui.get_element(document_id, element_id);
                  },
                  {"document", "element_id"})
            .func_sig(
                "add_event_listener",
                [&ui](UiElementId element_id, const std::string& event, sol::function fn) {
                    return ui.add_event_listener(element_id, event, [fn = std::move(fn)]() {
                        const sol::protected_function callback = fn;
                        const sol::protected_function_result result = callback();
                        if (!result.valid()) {
                            const sol::error err = result;
                            debug::log_error("UI event handler error: {}", err.what());
                        }
                    });
                },
                "(element: integer, event: string, fn: fun()): integer")
            .func("remove_event_listener",
                  [&ui](UiListenerId id) {
                      ui.remove_event_listener(id);
                  },
                  {"listener"})
            .func_sig(
                "create_model",
                [&ui](const std::string& name, sol::table fields) {
                    std::unordered_map<std::string, UiValue> initial;
                    for (const auto& [key, value] : fields) {
                        initial.emplace(key.as<std::string>(), lua_to_ui_value(value));
                    }
                    return ui.create_model(name, initial);
                },
                "(name: string, fields: table<string, boolean|integer|number|string>): integer")
            .func("destroy_model",
                  [&ui](UiDataModelId id) {
                      ui.destroy_model(id);
                  },
                  {"model"})
            .func_sig(
                "get",
                [&ui](UiDataModelId model, const std::string& field, sol::this_state ts) {
                    return ui_value_to_lua(sol::state_view(ts), ui.get_model_value(model, field));
                },
                "(model: integer, field: string): boolean|integer|number|string")
            .func_sig(
                "set",
                [&ui](UiDataModelId model, const std::string& field, sol::object value) {
                    ui.set_model_value(model, field, lua_to_ui_value(value));
                },
                "(model: integer, field: string, value: boolean|integer|number|string)")
            .func_sig(
                "bind_event",
                [&ui](UiDataModelId model, const std::string& event, sol::function fn) {
                    ui.bind_model_event(model, event, [fn = std::move(fn)]() {
                        const sol::protected_function callback = fn;
                        const sol::protected_function_result result = callback();
                        if (!result.valid()) {
                            const sol::error err = result;
                            debug::log_error("UI data-event handler error: {}", err.what());
                        }
                    });
                },
                "(model: integer, event: string, fn: fun())");
    }
} // namespace hob
