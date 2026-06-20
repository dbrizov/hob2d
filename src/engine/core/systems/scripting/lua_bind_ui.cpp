#include <string>

#include "engine/core/debug.h"
#include "engine/core/engine.h"
#include "engine/core/systems/ui/ui_system.h"
#include "lua_meta.h"
#include "lua_script_system.h"
#include "lua_script_system_impl.h"
#include "lua_type_names.h" // IWYU pragma: keep

namespace hob {
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
                  {"listener"});
    }
} // namespace hob
