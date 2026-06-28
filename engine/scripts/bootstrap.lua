-- Lua-side bootstrap. The C++ side runs ONLY this file; all script loading
-- (engine modules, user scripts, main.lua) is orchestrated from here.

-- Start the Lua debugger when launched under the VS Code Lua debugger extension.
if os.getenv("LOCAL_LUA_DEBUGGER_VSCODE") == "1" then
    require("lldebugger").start()
end

-- Engine modules (registries / metatables / enums) + generated path/factory registries.
-- lib/ and meta/ are excluded; hot_reload.lua is imperative, run on demand by C++.
function _G.__load_engine_modules()
    Scripts.run_engine_folder("scripts", { "bootstrap.lua", "hot_reload.lua", "lib", "meta" })
    __install_path_registries()
    __install_factory_registries()
end

-- All user definition files + resolve the component inheritance graph.
function _G.__load_project_definitions()
    Scripts.run_project_folder("scripts", { "main.lua", "meta" })
    __finalize_components()
end

__load_engine_modules()
__load_project_definitions()
__warmup_shaders()

-- Entry point.
Scripts.run_project_file("scripts/main.lua")
