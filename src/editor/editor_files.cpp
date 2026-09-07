#include "editor_files.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "editor.h"
#include "editor_file_dialog.h"
#include "editor_lua.h"
#include "editor_modal.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"
#include "engine/core/systems/scripting/lua_script_system.h"
#include "engine/core/systems/window.h"

namespace hob::editor {
    namespace {
        constexpr const char* SCENES_FOLDER = "scenes";

        constexpr const char* SCENE_FILE_FILTER_NAME = "Scene";

        constexpr const char* SCENE_CREATE_ERROR_TITLE = "Cannot Create Scene";
        constexpr const char* NEW_SCENE_DIALOG_TITLE = "New Scene";
        constexpr const char* SAVE_SCENE_AS_DIALOG_TITLE = "Save Scene As";

        bool write_file(const std::filesystem::path& path, const std::string& text) {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) {
                log::editor.error("Cannot open '{}' for writing", path.string());
                return false;
            }

            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.close();

            if (!out) {
                log::editor.error("Failed while writing '{}'", path.string());
                return false;
            }

            return true;
        }

        bool is_under(const std::filesystem::path& path, const std::filesystem::path& root) {
            std::error_code ec;

            const std::filesystem::path resolved_root = std::filesystem::weakly_canonical(root, ec);
            if (ec) {
                return false;
            }

            const std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ec);
            if (ec) {
                return false;
            }

            const std::filesystem::path relative = resolved.lexically_relative(resolved_root);

            return !relative.empty() && *relative.begin() != "..";
        }

        bool is_under_a_scanned_definition_root(const std::filesystem::path& path) {
            for (const auto& root : PathUtils::get_project_definition_roots()) {
                if (is_under(path, root)) {
                    return true;
                }
            }

            return false;
        }

        std::string describe_scanned_definition_roots() {
            std::string described;
            for (const auto& root : PathUtils::get_project_definition_roots()) {
                if (!described.empty()) {
                    described += " or ";
                }

                described += std::format("'{}'", root.string());
            }

            return described;
        }

        // The dialog filter spells extensions without the leading dot.
        std::string get_scene_file_filter_pattern() {
            return std::string(std::string_view(file_extension::SCENE).substr(1));
        }

        std::filesystem::path get_default_scene_folder(Editor& editor) {
            const sol::object file =
                editor_call(editor.get_engine(), editor_func::GET_SCENE_FILE, editor.get_current_scene());
            if (file.is<std::string>()) {
                return std::filesystem::path(file.as<std::string>()).parent_path();
            }

            const std::filesystem::path assets_root = PathUtils::get_project_assets_root();
            const std::filesystem::path scenes_folder = assets_root / SCENES_FOLDER;
            if (std::filesystem::exists(scenes_folder)) {
                return scenes_folder;
            }

            return std::filesystem::exists(assets_root) ? assets_root : PathUtils::get_project_scripts_root();
        }

        EditorFileDialogConfig make_scene_file_dialog_config(Editor& editor, const char* title) {
            return {
                .type = EditorFileDialogType::SaveFile,
                .title = title,
                .filters = {{.name = SCENE_FILE_FILTER_NAME, .pattern = get_scene_file_filter_pattern()}},
                .default_location = get_default_scene_folder(editor),
                .required_suffix = file_extension::SCENE,
                .parent_window = editor.get_engine().get_main_window().get_window(),
            };
        }

        void report_scene_create_error(Editor& editor, const std::filesystem::path& path, const std::string& reason) {
            log::editor.error("Cannot create a scene at '{}' because {}", path.string(), reason);

            editor.get_modal().open({
                .title = SCENE_CREATE_ERROR_TITLE,
                .message = std::format("Cannot create a scene at '{}'.", path.string()),
                .reason = reason,
                .buttons = {.confirm = "OK"},
            });
        }

        std::string get_scene_name_for_file(Engine& engine, const std::filesystem::path& path) {
            const sol::object name = editor_call(engine, editor_func::GET_SCENE_NAME_FOR_FILE, path.string());

            return name.is<std::string>() ? name.as<std::string>() : std::string();
        }

        void reload_after_write(Engine& engine) {
            LuaScriptSystem& lua_script_system = engine.get_lua_script_system();
            lua_script_system.hot_reload();
            lua_script_system.rebaseline_script_watch();
        }

        // The reload is what runs the file just written, which is what makes M8a's recorder stamp its
        // path before the scene is opened from it.
        void publish_new_scene(Editor& editor, const std::string& scene_name) {
            reload_after_write(editor.get_engine());
            editor.request_open_scene(scene_name);
        }

        std::optional<std::string> get_lua_save_error(Engine& engine, const char* func, const std::string& name) {
            if (!get_editor_func(engine, func).valid()) {
                return std::format("{} is unavailable", func);
            }

            const sol::object result = editor_call(engine, func, name);
            if (result.is<std::string>()) {
                return result.as<std::string>();
            }

            return std::nullopt;
        }

        std::optional<std::string> get_scene_save_error(const Editor& editor) {
            if (editor.get_current_scene().empty()) {
                return "no scene is open";
            }

            return get_lua_save_error(
                editor.get_engine(), editor_func::GET_SCENE_SAVE_ERROR, editor.get_current_scene());
        }

        std::optional<std::string> get_prefab_save_error(const Editor& editor, const std::string& prefab_name) {
            return get_lua_save_error(editor.get_engine(), editor_func::GET_PREFAB_SAVE_ERROR, prefab_name);
        }

        std::optional<std::filesystem::path> get_recorded_file(Engine& engine,
                                                               const char* get_file_func,
                                                               const std::string& name) {
            const sol::object file = editor_call(engine, get_file_func, name);
            if (!file.is<std::string>()) {
                log::editor.error("'{}' has no recorded source file", name);
                return std::nullopt;
            }

            return std::filesystem::path(file.as<std::string>());
        }

        bool write_definition(Engine& engine,
                              const char* get_file_func,
                              const char* serialize_func,
                              const std::string& name,
                              const char* kind) {
            const std::optional<std::filesystem::path> path = get_recorded_file(engine, get_file_func, name);
            if (!path.has_value()) {
                return false;
            }

            const sol::object source = editor_call(engine, serialize_func, name);
            if (!source.is<std::string>()) {
                return false;
            }

            if (!write_file(*path, source.as<std::string>())) {
                return false;
            }

            log::editor.info("Saved {} '{}' to '{}'", kind, name, path->string());

            return true;
        }

        bool write_scene(Editor& editor) {
            const std::optional<std::string> reason = get_scene_save_error(editor);
            if (reason.has_value()) {
                log::editor.error("Cannot save the scene because {}", *reason);
                return false;
            }

            Engine& engine = editor.get_engine();
            if (!write_definition(engine,
                                  editor_func::GET_SCENE_FILE,
                                  editor_func::SERIALIZE_SCENE,
                                  editor.get_current_scene(),
                                  "scene")) {
                return false;
            }

            editor_call(engine, editor_func::MARK_SCENE_SAVED);

            return true;
        }

        bool write_prefab(Editor& editor, const std::string& prefab_name) {
            const std::optional<std::string> reason = get_prefab_save_error(editor, prefab_name);
            if (reason.has_value()) {
                log::editor.error("Cannot save prefab '{}' because {}", prefab_name, *reason);
                return false;
            }

            Engine& engine = editor.get_engine();
            if (!write_definition(
                    engine, editor_func::GET_PREFAB_FILE, editor_func::SERIALIZE_PREFAB, prefab_name, "prefab")) {
                return false;
            }

            editor_call(engine, editor_func::MARK_PREFAB_SAVED, prefab_name);

            return true;
        }

        void revert_scene(Editor& editor) {
            Engine& engine = editor.get_engine();
            const std::string& scene_name = editor.get_current_scene();

            const std::optional<std::filesystem::path> path =
                get_recorded_file(engine, editor_func::GET_SCENE_FILE, scene_name);
            if (!path.has_value() || !engine.get_lua_script_system().run_file(*path)) {
                return;
            }

            editor_call(engine, editor_func::MARK_SCENE_SAVED);

            log::editor.info("Reverted scene '{}' from '{}'", scene_name, path->string());
        }

        void revert_prefab(Editor& editor, const std::string& prefab_name) {
            Engine& engine = editor.get_engine();

            const std::optional<std::filesystem::path> path =
                get_recorded_file(engine, editor_func::GET_PREFAB_FILE, prefab_name);
            if (!path.has_value() || !engine.get_lua_script_system().run_file(*path)) {
                return;
            }

            editor_call(engine, editor_func::MARK_PREFAB_REVERTED, prefab_name);

            log::editor.info("Reverted prefab '{}' from '{}'", prefab_name, path->string());
        }
    } // namespace

    std::optional<std::string> get_save_error(const Editor& editor) {
        if (editor.get_state() != WorldState::Stopped) {
            return "documents are only editable while the world is stopped";
        }

        if (editor.is_scene_dirty()) {
            const std::optional<std::string> reason = get_scene_save_error(editor);
            if (reason.has_value()) {
                return reason;
            }
        }

        for (const std::string& prefab_name : editor.get_dirty_prefab_names()) {
            const std::optional<std::string> reason = get_prefab_save_error(editor, prefab_name);
            if (reason.has_value()) {
                return reason;
            }
        }

        return std::nullopt;
    }

    bool can_save(const Editor& editor) {
        if (editor.get_state() != WorldState::Stopped) {
            return false;
        }

        return !get_scene_save_error(editor).has_value() || !editor.get_dirty_prefab_names().empty();
    }

    void save_all(Editor& editor) {
        if (editor.get_state() != WorldState::Stopped) {
            log::editor.error("Cannot save while the world is not stopped");
            return;
        }

        bool written = false;

        if (!editor.get_current_scene().empty()) {
            written = write_scene(editor) || written;
        }

        for (const std::string& prefab_name : editor.get_dirty_prefab_names()) {
            written = write_prefab(editor, prefab_name) || written;
        }

        if (written) {
            reload_after_write(editor.get_engine());
        }
    }

    void revert_all(Editor& editor) {
        if (editor.is_scene_dirty()) {
            revert_scene(editor);
        }

        for (const std::string& prefab_name : editor.get_dirty_prefab_names()) {
            revert_prefab(editor, prefab_name);
        }
    }

    bool can_new_scene(const Editor& editor) {
        return editor.get_state() == WorldState::Stopped;
    }

    // Deliberately weaker than can_save_scene: a scene with no recorded file, or one sharing its file
    // with another definition, is exactly what Save As exists to move out of.
    bool can_save_scene_as(const Editor& editor) {
        return !editor.get_current_scene().empty() && editor.get_state() == WorldState::Stopped;
    }

    void show_new_scene_dialog(Editor& editor) {
        EditorFileDialogConfig config = make_scene_file_dialog_config(editor, NEW_SCENE_DIALOG_TITLE);
        config.on_pick = [&editor](const std::filesystem::path& path) {
            new_scene(editor, path);
        };

        editor.get_file_dialog().open(std::move(config));
    }

    void show_save_scene_as_dialog(Editor& editor) {
        EditorFileDialogConfig config = make_scene_file_dialog_config(editor, SAVE_SCENE_AS_DIALOG_TITLE);
        config.on_pick = [&editor](const std::filesystem::path& path) {
            save_scene_as(editor, path);
        };

        editor.get_file_dialog().open(std::move(config));
    }

    std::optional<std::string> get_scene_create_error(const Editor& editor, const std::filesystem::path& path) {
        if (!is_under_a_scanned_definition_root(path)) {
            return std::format("a scene must live under {}", describe_scanned_definition_roots());
        }

        Engine& engine = editor.get_engine();
        if (!get_editor_func(engine, editor_func::GET_SCENE_CREATE_ERROR).valid()) {
            return std::format("{} is unavailable", editor_func::GET_SCENE_CREATE_ERROR);
        }

        const sol::object result = editor_call(engine, editor_func::GET_SCENE_CREATE_ERROR, path.string());
        if (result.is<std::string>()) {
            return result.as<std::string>();
        }

        return std::nullopt;
    }

    void new_scene(Editor& editor, const std::filesystem::path& path) {
        const std::optional<std::string> reason = get_scene_create_error(editor, path);
        if (reason.has_value()) {
            report_scene_create_error(editor, path, *reason);
            return;
        }

        Engine& engine = editor.get_engine();
        const std::string scene_name = get_scene_name_for_file(engine, path);
        if (scene_name.empty()) {
            log::editor.error("'{}' does not name a scene", path.string());
            return;
        }

        const sol::object source = editor_call(engine, editor_func::SERIALIZE_NEW_SCENE, scene_name);
        if (!source.is<std::string>()) {
            return;
        }

        if (!write_file(path, source.as<std::string>())) {
            return;
        }

        log::editor.info("Created scene '{}' in '{}'", scene_name, path.string());

        publish_new_scene(editor, scene_name);
    }

    void save_scene_as(Editor& editor, const std::filesystem::path& path) {
        const std::optional<std::string> reason = get_scene_create_error(editor, path);
        if (reason.has_value()) {
            report_scene_create_error(editor, path, *reason);
            return;
        }

        Engine& engine = editor.get_engine();
        const std::string source_scene_name = editor.get_current_scene();
        const std::string scene_name = get_scene_name_for_file(engine, path);
        if (scene_name.empty()) {
            log::editor.error("'{}' does not name a scene", path.string());
            return;
        }

        const sol::object source = editor_call(engine, editor_func::SERIALIZE_SCENE, source_scene_name, scene_name);
        if (!source.is<std::string>()) {
            return;
        }

        if (!write_file(path, source.as<std::string>())) {
            return;
        }

        // Before the reload, so rebind_instance_defs takes the file values of the scene being left
        // rather than the in-memory overrides, which now belong to the new file.
        editor_call(engine, editor_func::MARK_SCENE_SAVED);

        log::editor.info("Saved scene '{}' as '{}' in '{}'", source_scene_name, scene_name, path.string());

        publish_new_scene(editor, scene_name);
    }
} // namespace hob::editor
