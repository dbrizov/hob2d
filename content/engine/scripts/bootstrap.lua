-- Lua-side bootstrap.

-- Start the Lua debugger when launched under the VS Code Lua debugger extension.
if os.getenv("LOCAL_LUA_DEBUGGER_VSCODE") == "1" then
    require("lldebugger").start()
end

-- Excluded from the scan: hot_reload.lua is imperative (run on demand by C++), editor/ is run by
-- the editor host itself, and lib/ + meta/ are third-party code and type stubs.
function __load_engine_modules()
    Scripts.run_engine_folder("scripts", { "bootstrap.lua", "hot_reload.lua", "lib", "meta", "editor" })
    __install_asset_factories()
end

function __load_project_definitions()
    Scripts.run_project_folder("scripts", { "main.lua", "meta" })
    Scripts.run_project_folder("assets", { "meta" })
    __finalize_components()
end

__load_engine_modules()
__load_project_definitions()
__warmup_shaders()

-- Entry point. A host (the editor) can defer it from C++ and run main.lua itself later
if not __defer_project_main then
    Scripts.run_project_file("scripts/main.lua")
end
