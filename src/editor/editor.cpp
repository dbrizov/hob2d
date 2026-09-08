#include "editor.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include <ImGuizmo.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "actions/editor_action.h"
#include "editor_config.h"
#include "editor_files.h"
#include "editor_gui_utils.h"
#include "editor_lua.h"
#include "editor_modal.h"
#include "editor_style.h"
#include "engine/core/engine.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/window.h"

namespace hob::editor {
    namespace {
        constexpr const char* EDITOR_SCRIPTS_FOLDER = "scripts/editor";
        constexpr const char* UNSAVED_CHANGES_TITLE = "Unsaved Changes";

        constexpr float LAYOUT_RIGHT_COLUMNS_RATIO = 0.46f;
        constexpr float LAYOUT_INSPECTOR_RATIO = 0.50f;
        constexpr float LAYOUT_ASSETS_RATIO = 0.50f;
        constexpr float LAYOUT_OUTPUT_RATIO = 0.30f;

        std::string describe_unsaved_changes(const Editor& editor) {
            std::vector<std::string> documents;
            if (editor.is_scene_dirty()) {
                documents.push_back(std::format("scene '{}'", editor.get_current_scene()));
            }

            for (const std::string& prefab_name : editor.get_dirty_prefab_names()) {
                documents.push_back(std::format("prefab '{}'", prefab_name));
            }

            std::string described;
            for (size_t i = 0; i < documents.size(); ++i) {
                if (i > 0) {
                    described += (i + 1 == documents.size()) ? " and " : ", ";
                }

                described += documents[i];
            }

            return std::format("{} {} unsaved changes.", described, documents.size() == 1 ? "has" : "have");
        }
    } // namespace

    Editor::Editor(Engine& engine)
        : m_engine(engine)
        , m_imgui_ini_path(get_editor_imgui_ini_file_path().string()) {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = m_imgui_ini_path.c_str();
        io.ConfigDragClickToInputText = true;

        const EditorConfig editor_config(get_editor_config_file_path());
        m_pending_scene_open = editor_config.last_open_scene;

        if (editor_config.game_window.has_size()) {
            WindowConfig game_window_config = m_engine.get_game_window_config();
            apply_editor_window_config(editor_config.game_window, game_window_config);
            m_engine.set_game_window_config(game_window_config);
        }

        m_engine.get_lua_script_system().run_engine_folder(EDITOR_SCRIPTS_FOLDER);

        apply_style();
        m_icons.load(m_engine.get_renderer());
        m_engine.get_imgui_system().set_clear_color(COLOR_CLEAR);
        m_engine.get_imgui_system().set_clear_swapchain(true); // Nothing but ImGui draws to the editor window.

        m_reset_layout = !std::filesystem::exists(m_imgui_ini_path);

        m_engine.set_world_state(WorldState::Stopped);

        log::editor.info("Editor::Initialise");
    }

    Editor::~Editor() {
        ImGuiIO& io = ImGui::GetIO();
        if (io.IniFilename != nullptr) {
            ImGui::SaveIniSettingsToDisk(io.IniFilename);
            io.IniFilename = nullptr;
        }

        save_layout();
        m_scene_view.release_color_target(*this);
        m_inspector.reset_edit_state();
        clear_lua_query_caches();

        log::editor.info("Editor::Shutdown");
    }

    Engine& Editor::get_engine() const {
        return m_engine;
    }

    WorldState Editor::get_state() const {
        return m_engine.get_world_state();
    }

    void Editor::set_state(WorldState state) {
        const WorldState previous = get_state();
        if (state == previous) {
            return;
        }

        const bool entering_play = (previous == WorldState::Stopped);
        const bool leaving_play = (state == WorldState::Stopped);

        if (entering_play && has_unsaved_changes()) {
            save_all(*this);
        }

        if (entering_play || leaving_play) {
            const EditorSelectionInstanceIds captured = capture_selection_instance_ids();

            reset_edit_session();
            clear_world();

            if (entering_play) {
                m_engine.open_game_window();
            }
            else {
                m_engine.close_game_window();
            }

            load_scene();
            restore_selection(captured);
        }

        m_engine.set_world_state(state);
    }

    void Editor::request_step() {
        m_engine.request_tick_step();
    }

    void Editor::request_reset_layout() {
        m_reset_layout = true;
    }

    void Editor::request_quit() {
        SDL_Event quit_event{};
        quit_event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit_event);
    }

    void Editor::request_action(EditorActionId id) {
        m_actions.request(id);
    }

    void Editor::request_open_scene(const std::string& name) {
        if (!has_unsaved_changes() || get_state() != WorldState::Stopped) {
            open_scene_without_prompt(name);
            return;
        }

        prompt_unsaved_changes(
            [this] {
                revert_all(*this);
            },
            [this, name] {
                open_scene_without_prompt(name);
            });
    }

    void Editor::request_prefab_delete(const std::string& prefab_name) {
        m_pending_prefab_delete = prefab_name;
        request_action(EditorActionId::DeletePrefab);
    }

    std::vector<std::string> Editor::get_scene_names() const {
        std::vector<std::string> names;

        const sol::object result = editor_call(m_engine, editor_func::GET_SCENE_NAMES);
        if (result.is<sol::table>()) {
            const sol::table table = result.as<sol::table>();
            names.reserve(table.size());
            for (int32_t i = 1; i <= table.size(); ++i) {
                names.push_back(table.get<std::string>(i));
            }
        }

        return names;
    }

    const std::string& Editor::get_current_scene() const {
        return m_current_scene;
    }

    Space Editor::get_scene_space() const {
        return m_scene_space;
    }

    void Editor::refresh_scene_space() {
        const sol::object space = editor_call(m_engine, editor_func::GET_SCENE_SPACE);
        m_scene_space =
            space.is<std::string>() ? space_from_key(space.as<std::string>()).value_or(Space::Space2D) : Space::Space2D;
    }

    void Editor::open_pending_scene() {
        if (m_pending_scene_open.empty()) {
            return;
        }

        const std::string name = std::move(m_pending_scene_open);
        m_pending_scene_open.clear();

        const sol::object result = editor_call(m_engine, editor_func::OPEN_SCENE, name);
        if (!result.is<bool>() || !result.as<bool>()) {
            clear_world();
            m_current_scene.clear();
            m_scene_space = Space::Space2D;
            reset_edit_session();
            return;
        }

        m_current_scene = name;
        refresh_scene_space();
        reset_edit_session();
    }

    std::vector<std::string> Editor::get_dirty_prefab_names() const {
        std::vector<std::string> names;

        const sol::object result = editor_call(m_engine, editor_func::GET_DIRTY_PREFAB_NAMES);
        if (result.is<sol::table>()) {
            const sol::table table = result.as<sol::table>();
            names.reserve(table.size());
            for (int32_t i = 1; i <= table.size(); ++i) {
                names.push_back(table.get<std::string>(i));
            }
        }

        return names;
    }

    void Editor::respawn_prefab_instances(const std::string& prefab_name) {
        const EditorSelectionInstanceIds captured = capture_selection_instance_ids();

        editor_call(m_engine, editor_func::RESPAWN_PREFAB_INSTANCES, prefab_name);

        m_selection.ids.clear();
        m_selection.range_anchor = INVALID_ENTITY_ID;
        restore_selection(captured);

        m_scene_view.reset_pick_cycle();
        m_scene_view.reset_gizmo();
        m_inspector.reset_edit_state();
    }

    void Editor::delete_pending_prefab() {
        if (m_pending_prefab_delete.empty()) {
            return;
        }

        const std::string prefab_name = std::move(m_pending_prefab_delete);
        m_pending_prefab_delete.clear();

        delete_prefab(*this, prefab_name);
    }

    bool Editor::is_scene_dirty() const {
        const sol::object result = editor_call(m_engine, editor_func::IS_SCENE_DIRTY);
        return result.is<bool>() && result.as<bool>();
    }

    bool Editor::has_unsaved_changes() const {
        return is_scene_dirty() || !get_dirty_prefab_names().empty();
    }

    EditorSelection& Editor::get_selection() {
        return m_selection;
    }

    const EditorSelection& Editor::get_selection() const {
        return m_selection;
    }

    EditorCommandStack& Editor::get_commands() {
        return m_commands;
    }

    const EditorCommandStack& Editor::get_commands() const {
        return m_commands;
    }

    const EditorIcons& Editor::get_icons() const {
        return m_icons;
    }

    EditorModal& Editor::get_modal() {
        return m_modal;
    }

    const EditorModal& Editor::get_modal() const {
        return m_modal;
    }

    EditorFileDialog& Editor::get_file_dialog() {
        return m_file_dialog;
    }

    const EditorFileDialog& Editor::get_file_dialog() const {
        return m_file_dialog;
    }

    EditorMenuBar& Editor::get_menu_bar() {
        return m_menu_bar;
    }

    const EditorMenuBar& Editor::get_menu_bar() const {
        return m_menu_bar;
    }

    EditorToolbar& Editor::get_toolbar() {
        return m_toolbar;
    }

    const EditorToolbar& Editor::get_toolbar() const {
        return m_toolbar;
    }

    EditorDockSceneView& Editor::get_scene_view() {
        return m_scene_view;
    }

    const EditorDockSceneView& Editor::get_scene_view() const {
        return m_scene_view;
    }

    EditorDockHierarchy& Editor::get_hierarchy() {
        return m_hierarchy;
    }

    const EditorDockHierarchy& Editor::get_hierarchy() const {
        return m_hierarchy;
    }

    EditorDockInspector& Editor::get_inspector() {
        return m_inspector;
    }

    const EditorDockInspector& Editor::get_inspector() const {
        return m_inspector;
    }

    EditorDockAssets& Editor::get_assets() {
        return m_assets;
    }

    const EditorDockAssets& Editor::get_assets() const {
        return m_assets;
    }

    EditorDockOutput& Editor::get_output() {
        return m_output;
    }

    const EditorDockOutput& Editor::get_output() const {
        return m_output;
    }

    void Editor::init() {
        const std::vector<std::string> names = get_scene_names();
        if (names.empty()) {
            log::editor.info("The project defines no scenes; starting with an empty world");
            return;
        }

        if (std::find(names.begin(), names.end(), m_pending_scene_open) == names.end()) {
            m_pending_scene_open = names.front();
        }

        open_pending_scene();
    }

    void Editor::end_frame() {
        m_assets.poll(*this);
        m_file_dialog.poll();
        m_actions.flush(*this);
    }

    void Editor::tick(float delta_time) {
        update_input();
        update_window_title();

        prune_selection();
    }

    void Editor::draw_gui() {
        ImGuizmo::BeginFrame();

        if (ImGui::BeginMainMenuBar()) {
            m_menu_bar.draw(*this);
            m_toolbar.draw(*this);
            ImGui::EndMainMenuBar();
        }

        const ImGuiID dock_space_id = dock_space_over_viewport(ImGuiDockNodeFlags_PassthruCentralNode);
        if (m_reset_layout) {
            m_reset_layout = false;
            build_default_layout(dock_space_id);
        }

        for (EditorDock* dock : get_docks()) {
            dock->draw(*this);
        }

        m_modal.draw();
    }

    void Editor::render_passes() {
        m_scene_view.render_pass(*this);
    }

    void Editor::on_lua_hot_reloaded() {
        m_engine.get_lua_script_system().run_engine_folder(EDITOR_SCRIPTS_FOLDER);
        clear_lua_query_caches();
        m_assets.request_rebuild();

        editor_call(m_engine, editor_func::REBIND_PREFAB_DEFS);

        const sol::object rebound = editor_call(m_engine, editor_func::REBIND_INSTANCE_DEFS);
        if (rebound.is<bool>() && !rebound.as<bool>()) {
            open_scene_without_prompt(m_current_scene);
        }
        else {
            refresh_scene_space();
        }
    }

    bool Editor::on_quit_requested() {
        const Window* game_window = m_engine.get_game_window();
        if (game_window != nullptr && game_window->has_focus()) {
            set_state(WorldState::Stopped);
            return true;
        }

        return try_prompt_unsaved_changes();
    }

    bool Editor::on_window_close_requested(SDL_WindowID window_id) {
        const Window* game_window = m_engine.get_game_window();
        if (game_window != nullptr && game_window->get_id() == window_id) {
            set_state(WorldState::Stopped);
            return true;
        }

        return try_prompt_unsaved_changes();
    }

    std::array<EditorDock*, Editor::DOCK_COUNT> Editor::get_docks() {
        return {&m_scene_view, &m_hierarchy, &m_inspector, &m_assets, &m_output};
    }

    void Editor::update_input() {
        m_active_contexts = 0;

        const bool has_focus = m_engine.get_main_window().has_focus() || m_engine.get_play_window().has_focus();
        if (has_focus && !ImGui::GetIO().WantTextInput && !m_scene_view.is_flying()) {
            m_active_contexts |= context_bit(EditorActionContext::Global);

            // Last frame's, since draw() writes them after this runs and a dock rect only exists mid-draw.
            for (const EditorDock* dock : get_docks()) {
                if (dock->is_hovered() || dock->is_focused()) {
                    m_active_contexts |= context_bit(dock->get_context());
                }
            }

            for (const EditorAction& action : get_actions()) {
                if (is_context_active(action.context) && is_chord_pressed(action.chord) &&
                    is_action_enabled(*this, action.id)) {
                    m_actions.request(action.id);
                }
            }
        }

        m_scene_view.update_input(*this);
    }

    void Editor::update_window_title() {
        const std::string project = PathUtils::get_project_root().filename().string();

        std::string title;
        if (has_unsaved_changes()) {
            title = std::string(DIRTY_MARKER) + " ";
        }

        if (!m_current_scene.empty()) {
            title += m_current_scene;
            title += " - ";
        }

        title += project;
        title += " - ";
        title += EDITOR_WINDOW_TITLE;

        if (title != m_window_title) {
            m_window_title = title;
            m_engine.get_main_window().set_title(m_window_title);
        }
    }

    bool Editor::is_context_active(EditorActionContext context) const {
        return (m_active_contexts & context_bit(context)) != 0;
    }

    void Editor::open_scene_without_prompt(const std::string& name) {
        m_pending_scene_open = name;
        request_action(EditorActionId::OpenScene);
    }

    void Editor::quit_without_prompting() {
        m_quit_confirmed = true;
        request_quit();
    }

    bool Editor::try_prompt_unsaved_changes() {
        if (m_quit_confirmed) {
            return false;
        }

        if (m_modal.is_open()) {
            return true;
        }

        if (!has_unsaved_changes() || get_state() != WorldState::Stopped) {
            return false;
        }

        prompt_unsaved_changes(nullptr, [this] {
            quit_without_prompting();
        });
        return true;
    }

    void Editor::prompt_unsaved_changes(std::function<void()> revert_changes, std::function<void()> proceed) {
        const std::optional<std::string> save_error = get_save_error(*this);

        m_modal.open({
            .title = UNSAVED_CHANGES_TITLE,
            .message = describe_unsaved_changes(*this),
            .reason = save_error,
            .buttons = {.confirm = "Save",
                        .discard = "Don't Save",
                        .cancel = "Cancel",
                        .is_confirm_enabled = !save_error.has_value()},
            .on_confirm =
                [this, proceed] {
                    request_action(EditorActionId::Save);
                    proceed();
                },
            .on_discard =
                [revert_changes, proceed] {
                    if (revert_changes) {
                        revert_changes();
                    }
                    proceed();
                },
        });
    }

    void Editor::prune_selection() {
        const EntitySpawner& spawner = m_engine.get_entity_spawner();

        std::erase_if(m_selection.ids, [&spawner](EntityId id) {
            return spawner.get_entity(id) == nullptr;
        });

        if (m_selection.range_anchor != INVALID_ENTITY_ID && spawner.get_entity(m_selection.range_anchor) == nullptr) {
            m_selection.range_anchor = INVALID_ENTITY_ID;
        }

        if (m_selection.definition.is_valid() && find_definition(m_engine, m_selection.definition) == nullptr) {
            m_selection.definition = EditorDefinitionRef{};
        }
    }

    EditorSelectionInstanceIds Editor::capture_selection_instance_ids() const {
        EditorSelectionInstanceIds captured;
        captured.ids.reserve(m_selection.ids.size());

        for (EntityId entity_id : m_selection.ids) {
            const EditorInstanceId instance_id = get_instance_id_of_entity(m_engine, entity_id);
            if (instance_id != INVALID_EDITOR_INSTANCE_ID) {
                captured.ids.push_back(instance_id);
            }
        }

        if (m_selection.range_anchor != INVALID_ENTITY_ID) {
            captured.range_anchor = get_instance_id_of_entity(m_engine, m_selection.range_anchor);
        }

        return captured;
    }

    void Editor::restore_selection(const EditorSelectionInstanceIds& captured) {
        for (EditorInstanceId instance_id : captured.ids) {
            const EntityId entity_id = get_entity_id_of_instance(m_engine, instance_id);
            if (entity_id != INVALID_ENTITY_ID) {
                m_selection.add(entity_id);
            }
        }

        if (captured.range_anchor != INVALID_EDITOR_INSTANCE_ID) {
            const EntityId anchor = get_entity_id_of_instance(m_engine, captured.range_anchor);
            if (anchor != INVALID_ENTITY_ID) {
                m_selection.range_anchor = anchor;
            }
        }
    }

    void Editor::reset_edit_session() {
        m_commands.clear();
        m_selection.clear();
        m_scene_view.reset_pick_cycle();
        m_scene_view.reset_gizmo();
        m_inspector.reset_edit_state();
    }

    void Editor::clear_world() {
        editor_call(m_engine, editor_func::CLEAR_WORLD);
    }

    void Editor::load_scene() {
        editor_call(m_engine, editor_func::LOAD_SCENE);
    }

    void Editor::build_default_layout(ImGuiID dock_space_id) {
        const ImGuiDockNodeFlags node_flags =
            static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace) | ImGuiDockNodeFlags_PassthruCentralNode;

        ImGui::DockBuilderRemoveNode(dock_space_id);
        ImGui::DockBuilderAddNode(dock_space_id, node_flags);
        ImGui::DockBuilderSetNodeSize(dock_space_id, ImGui::GetMainViewport()->Size);

        ImGuiID center = dock_space_id;
        ImGuiID hierarchy =
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, LAYOUT_RIGHT_COLUMNS_RATIO, nullptr, &center);
        const ImGuiID inspector =
            ImGui::DockBuilderSplitNode(hierarchy, ImGuiDir_Right, LAYOUT_INSPECTOR_RATIO, nullptr, &hierarchy);
        const ImGuiID assets =
            ImGui::DockBuilderSplitNode(hierarchy, ImGuiDir_Down, LAYOUT_ASSETS_RATIO, nullptr, &hierarchy);
        const ImGuiID output =
            ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, LAYOUT_OUTPUT_RATIO, nullptr, &center);

        ImGui::DockBuilderDockWindow(m_hierarchy.get_name().c_str(), hierarchy);
        ImGui::DockBuilderDockWindow(m_inspector.get_name().c_str(), inspector);
        ImGui::DockBuilderDockWindow(m_assets.get_name().c_str(), assets);
        ImGui::DockBuilderDockWindow(m_output.get_name().c_str(), output);
        ImGui::DockBuilderDockWindow(m_scene_view.get_name().c_str(), center);

        ImGui::DockBuilderFinish(dock_space_id);
    }

    void Editor::save_layout() {
        EditorConfig editor_config;
        editor_config.main_window = create_editor_window_config_from_window(m_engine.get_main_window());
        editor_config.game_window = create_editor_window_config_from_window_config(m_engine.get_game_window_config());
        editor_config.last_open_scene = m_current_scene;
        editor_config.save(get_editor_config_file_path());
    }
} // namespace hob::editor
