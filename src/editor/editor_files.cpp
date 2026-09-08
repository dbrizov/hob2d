#include "editor_files.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "commands/editor_command_add_instance.h"
#include "commands/editor_command_composite.h"
#include "commands/editor_command_remove_instance.h"
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
        struct EditorDefinitionFileKind {
            const char* noun;
            const char* filter_name;
            const char* extension;
            const char* create_error_title;
            const char* name_for_file_func;
            const char* create_error_func;
        };

        constexpr EditorDefinitionFileKind SCENE_FILE_KIND{
            .noun = "scene",
            .filter_name = "Scene",
            .extension = file_extension::SCENE,
            .create_error_title = "Cannot Create Scene",
            .name_for_file_func = editor_func::GET_SCENE_NAME_FOR_FILE,
            .create_error_func = editor_func::GET_SCENE_CREATE_ERROR,
        };

        constexpr EditorDefinitionFileKind PREFAB_FILE_KIND{
            .noun = "prefab",
            .filter_name = "Prefab",
            .extension = file_extension::PREFAB,
            .create_error_title = "Cannot Create Prefab",
            .name_for_file_func = editor_func::GET_PREFAB_NAME_FOR_FILE,
            .create_error_func = editor_func::GET_PREFAB_CREATE_ERROR,
        };

        constexpr const char* NEW_SCENE_DIALOG_TITLE = "New Scene";
        constexpr const char* NEW_SCENE_3D_DIALOG_TITLE = "New 3D Scene";
        constexpr const char* SAVE_SCENE_AS_DIALOG_TITLE = "Save Scene As";
        constexpr const char* NEW_PREFAB_DIALOG_TITLE = "New Prefab";
        constexpr const char* CREATE_PREFAB_COMMAND_LABEL = "Create Prefab";
        constexpr const char* DELETE_PREFAB_TITLE = "Delete Prefab";
        constexpr const char* DELETE_PREFAB_ERROR_TITLE = "Cannot Delete Prefab";

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
        std::string get_file_filter_pattern(const EditorDefinitionFileKind& kind) {
            return std::string(std::string_view(kind.extension).substr(1));
        }

        std::filesystem::path get_default_folder() {
            const std::filesystem::path& assets_root = PathUtils::get_project_assets_root();
            return std::filesystem::exists(assets_root) ? assets_root : PathUtils::get_project_scripts_root();
        }

        std::filesystem::path get_open_scene_folder(Editor& editor) {
            const sol::object file =
                editor_call(editor.get_engine(), editor_func::GET_SCENE_FILE, editor.get_current_scene());
            if (file.is<std::string>()) {
                return std::filesystem::path(file.as<std::string>()).parent_path();
            }

            return get_default_folder();
        }

        EditorFileDialogConfig make_file_dialog_config(Editor& editor,
                                                       const EditorDefinitionFileKind& kind,
                                                       const char* title,
                                                       std::filesystem::path default_location) {
            return {
                .type = EditorFileDialogType::SaveFile,
                .title = title,
                .filters = {{.name = kind.filter_name, .pattern = get_file_filter_pattern(kind)}},
                .default_location = std::move(default_location),
                .required_suffix = kind.extension,
                .parent_window = editor.get_engine().get_main_window().get_window(),
            };
        }

        void report_create_error(Editor& editor,
                                 const EditorDefinitionFileKind& kind,
                                 const std::filesystem::path& path,
                                 const std::string& reason) {
            log::editor.error("Cannot create a {} at '{}' because {}", kind.noun, path.string(), reason);

            editor.get_modal().open({
                .title = kind.create_error_title,
                .message = std::format("Cannot create a {} at '{}'.", kind.noun, path.string()),
                .reason = reason,
                .buttons = {.confirm = "OK"},
            });
        }

        std::string get_definition_name_for_file(Engine& engine,
                                                 const EditorDefinitionFileKind& kind,
                                                 const std::filesystem::path& path) {
            const sol::object name = editor_call(engine, kind.name_for_file_func, path.string());

            return name.is<std::string>() ? name.as<std::string>() : std::string();
        }

        std::optional<std::string> get_definition_create_error(const Editor& editor,
                                                               const EditorDefinitionFileKind& kind,
                                                               const std::filesystem::path& path) {
            if (!is_under_a_scanned_definition_root(path)) {
                return std::format("a {} must live under {}", kind.noun, describe_scanned_definition_roots());
            }

            Engine& engine = editor.get_engine();
            if (!get_editor_func(engine, kind.create_error_func).valid()) {
                return std::format("{} is unavailable", kind.create_error_func);
            }

            const sol::object result = editor_call(engine, kind.create_error_func, path.string());
            if (result.is<std::string>()) {
                return result.as<std::string>();
            }

            return std::nullopt;
        }

        std::string create_definition_file(Editor& editor,
                                           const EditorDefinitionFileKind& kind,
                                           const std::filesystem::path& path,
                                           const std::function<sol::object(const std::string& name)>& serialize) {
            const std::optional<std::string> reason = get_definition_create_error(editor, kind, path);
            if (reason.has_value()) {
                report_create_error(editor, kind, path, *reason);
                return std::string();
            }

            const std::string name = get_definition_name_for_file(editor.get_engine(), kind, path);
            if (name.empty()) {
                log::editor.error("'{}' does not name a {}", path.string(), kind.noun);
                return std::string();
            }

            const sol::object source = serialize(name);
            if (!source.is<std::string>() || !write_file(path, source.as<std::string>())) {
                return std::string();
            }

            log::editor.info("Created {} '{}' in '{}'", kind.noun, name, path.string());

            return name;
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

        struct EditorPrefabReferrer {
            std::string scene;
            int32_t count = 0;
        };

        std::vector<EditorPrefabReferrer> get_prefab_referrers(Engine& engine, const std::string& prefab_name) {
            std::vector<EditorPrefabReferrer> referrers;

            const sol::object result = editor_call(engine, editor_func::GET_PREFAB_REFERRERS, prefab_name);
            if (!result.is<sol::table>()) {
                return referrers;
            }

            const sol::table rows = result.as<sol::table>();
            for (int32_t i = 1; i <= static_cast<int32_t>(rows.size()); ++i) {
                const sol::object row = rows[i];
                if (row.is<sol::table>()) {
                    const sol::table entry = row.as<sol::table>();
                    referrers.push_back({.scene = entry.get_or<std::string>(query_key::SCENE, ""),
                                         .count = entry.get_or(query_key::COUNT, 0)});
                }
            }

            return referrers;
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

    void show_new_scene_dialog(Editor& editor, Space space) {
        EditorFileDialogConfig config =
            make_file_dialog_config(editor,
                                    SCENE_FILE_KIND,
                                    space == Space::Space3D ? NEW_SCENE_3D_DIALOG_TITLE : NEW_SCENE_DIALOG_TITLE,
                                    get_default_folder());
        config.on_pick = [&editor, space](const std::filesystem::path& path) {
            new_scene(editor, path, space);
        };

        editor.get_file_dialog().open(std::move(config));
    }

    void show_save_scene_as_dialog(Editor& editor) {
        EditorFileDialogConfig config =
            make_file_dialog_config(editor, SCENE_FILE_KIND, SAVE_SCENE_AS_DIALOG_TITLE, get_open_scene_folder(editor));
        config.on_pick = [&editor](const std::filesystem::path& path) {
            save_scene_as(editor, path);
        };

        editor.get_file_dialog().open(std::move(config));
    }

    std::optional<std::string> get_scene_create_error(const Editor& editor, const std::filesystem::path& path) {
        return get_definition_create_error(editor, SCENE_FILE_KIND, path);
    }

    void new_scene(Editor& editor, const std::filesystem::path& path, Space space) {
        Engine& engine = editor.get_engine();
        const char* space_key = space_to_key(space);

        const std::string scene_name =
            create_definition_file(editor, SCENE_FILE_KIND, path, [&engine, space_key](const std::string& name) {
                return editor_call(engine, editor_func::SERIALIZE_NEW_SCENE, name, space_key);
            });
        if (scene_name.empty()) {
            return;
        }

        publish_new_scene(editor, scene_name);
    }

    void save_scene_as(Editor& editor, const std::filesystem::path& path) {
        Engine& engine = editor.get_engine();
        const std::string source_scene_name = editor.get_current_scene();

        const std::string scene_name = create_definition_file(
            editor, SCENE_FILE_KIND, path, [&engine, &source_scene_name](const std::string& name) {
                return editor_call(engine, editor_func::SERIALIZE_SCENE, source_scene_name, name);
            });
        if (scene_name.empty()) {
            return;
        }

        // Before the reload, so rebind_instance_defs takes the file values of the scene being left
        // rather than the in-memory overrides, which now belong to the new file.
        editor_call(engine, editor_func::MARK_SCENE_SAVED);

        log::editor.info("Saved scene '{}' as '{}'", source_scene_name, scene_name);

        publish_new_scene(editor, scene_name);
    }

    bool can_new_prefab(const Editor& editor) {
        return editor.get_state() == WorldState::Stopped;
    }

    void show_new_prefab_dialog(Editor& editor) {
        EditorFileDialogConfig config =
            make_file_dialog_config(editor, PREFAB_FILE_KIND, NEW_PREFAB_DIALOG_TITLE, get_default_folder());
        config.on_pick = [&editor](const std::filesystem::path& path) {
            new_prefab(editor, path);
        };

        editor.get_file_dialog().open(std::move(config));
    }

    std::optional<std::string> get_prefab_create_error(const Editor& editor, const std::filesystem::path& path) {
        return get_definition_create_error(editor, PREFAB_FILE_KIND, path);
    }

    void new_prefab(Editor& editor, const std::filesystem::path& path) {
        Engine& engine = editor.get_engine();

        const std::string prefab_name =
            create_definition_file(editor, PREFAB_FILE_KIND, path, [&engine](const std::string& name) {
                return editor_call(engine, editor_func::SERIALIZE_NEW_PREFAB, name);
            });
        if (prefab_name.empty()) {
            return;
        }

        reload_after_write(engine);
        editor.get_selection().select_definition({.registry = def_registry::ENTITIES, .name = prefab_name});
    }

    void create_prefab_from_entity_in_folder(Editor& editor, EntityId entity_id, const std::filesystem::path& folder) {
        Engine& engine = editor.get_engine();

        const Entity* entity = engine.get_entity_spawner().get_entity(entity_id);
        if (entity == nullptr) {
            return;
        }

        const std::string& base_name = entity->get_name().empty() ? entity->get_prefab_name() : entity->get_name();
        const sol::object name = editor_call(engine, editor_func::GET_UNIQUE_PREFAB_NAME, base_name);
        if (!name.is<std::string>()) {
            return;
        }

        create_prefab_from_entity(editor, entity_id, folder / (name.as<std::string>() + file_extension::PREFAB));
    }

    void create_prefab_from_entity(Editor& editor, EntityId entity_id, const std::filesystem::path& path) {
        Engine& engine = editor.get_engine();

        const EditorInstanceId instance_id = get_instance_id_of_entity(engine, entity_id);
        if (editor.get_state() != WorldState::Stopped || instance_id == INVALID_EDITOR_INSTANCE_ID) {
            return;
        }

        const sol::object def = editor_call(engine, editor_func::CREATE_PREFAB_DEF_FROM_ENTITY, entity_id);
        if (!def.is<sol::table>()) {
            return;
        }

        const std::string prefab_name =
            create_definition_file(editor, PREFAB_FILE_KIND, path, [&engine, &def](const std::string& name) {
                return editor_call(engine, editor_func::SERIALIZE_PREFAB_DEF, def, name);
            });
        if (prefab_name.empty()) {
            return;
        }

        // The reload is what registers the new prefab, so the instance can only be re-pointed after it.
        reload_after_write(engine);

        const sol::object old_instance = editor_call(engine, editor_func::GET_INSTANCE_DEF, instance_id);
        const sol::object new_instance =
            editor_call(engine, editor_func::REPOINT_INSTANCE_DEF, instance_id, prefab_name);
        const sol::object index = editor_call(engine, editor_func::GET_INSTANCE_INDEX, instance_id);
        if (!old_instance.is<sol::table>() || !new_instance.is<sol::table>() || !index.is<int32_t>()) {
            return;
        }

        std::vector<std::unique_ptr<EditorCommand>> commands;
        commands.push_back(std::make_unique<EditorCommandRemoveInstance>(
            CREATE_PREFAB_COMMAND_LABEL, old_instance.as<sol::table>(), instance_id));
        commands.push_back(std::make_unique<EditorCommandAddInstance>(
            CREATE_PREFAB_COMMAND_LABEL, new_instance.as<sol::table>(), index.as<int32_t>()));

        editor.get_commands().push(
            editor, std::make_unique<EditorCommandComposite>(CREATE_PREFAB_COMMAND_LABEL, std::move(commands)));
    }

    bool can_delete_selected_prefab(const Editor& editor) {
        const EditorDefinitionRef& definition = editor.get_selection().definition;
        return editor.get_state() == WorldState::Stopped && definition.registry == def_registry::ENTITIES;
    }

    void request_delete_selected_prefab(Editor& editor) {
        if (can_delete_selected_prefab(editor)) {
            request_delete_prefab(editor, editor.get_selection().definition.name);
        }
    }

    void request_delete_prefab(Editor& editor, const std::string& prefab_name) {
        Engine& engine = editor.get_engine();

        std::optional<std::string> reason;
        if (editor.get_state() != WorldState::Stopped) {
            reason = "documents are only editable while the world is stopped";
        }
        else {
            reason = get_prefab_save_error(editor, prefab_name);
        }

        const std::vector<EditorPrefabReferrer> referrers = get_prefab_referrers(engine, prefab_name);
        if (!reason.has_value() && !referrers.empty()) {
            std::string used_by;
            for (const EditorPrefabReferrer& referrer : referrers) {
                used_by += std::format(
                    "{} instance{} in scene '{}'\n", referrer.count, referrer.count == 1 ? "" : "s", referrer.scene);
            }

            reason = std::format("it is still used by:\n{}Remove those instances first.", used_by);
        }

        if (reason.has_value()) {
            log::editor.error("Cannot delete prefab '{}' because {}", prefab_name, *reason);
            editor.get_modal().open({
                .title = DELETE_PREFAB_ERROR_TITLE,
                .message = std::format("Cannot delete prefab '{}'.", prefab_name),
                .reason = reason,
                .buttons = {.confirm = "OK"},
            });
            return;
        }

        editor.get_modal().open({
            .title = DELETE_PREFAB_TITLE,
            .message = std::format("Delete prefab '{}' and its file?", prefab_name),
            .reason = "This cannot be undone.",
            .buttons = {.confirm = "Delete", .cancel = "Cancel"},
            .on_confirm =
                [&editor, prefab_name] {
                    editor.request_prefab_delete(prefab_name);
                },
        });
    }

    void delete_prefab(Editor& editor, const std::string& prefab_name) {
        Engine& engine = editor.get_engine();

        const std::optional<std::filesystem::path> path =
            get_recorded_file(engine, editor_func::GET_PREFAB_FILE, prefab_name);
        if (!path.has_value()) {
            return;
        }

        std::error_code error;
        if (!std::filesystem::remove(*path, error)) {
            log::editor.error("Cannot delete '{}': {}", path->string(), error.message());
            return;
        }

        editor_call(engine, editor_func::MARK_PREFAB_SAVED, prefab_name);

        EditorSelection& selection = editor.get_selection();
        if (selection.definition.registry == def_registry::ENTITIES && selection.definition.name == prefab_name) {
            selection.clear();
        }

        log::editor.info("Deleted prefab '{}' and '{}'", prefab_name, path->string());

        reload_after_write(engine);
    }
} // namespace hob::editor
