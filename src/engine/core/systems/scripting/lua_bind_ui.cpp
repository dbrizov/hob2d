#include <string>

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
                  {"doc"})
            .func("hide_document",
                  [&ui](UiDocumentId id) {
                      ui.hide_document(id);
                  },
                  {"doc"});
    }
} // namespace hob
