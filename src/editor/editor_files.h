#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "engine/entity/entity.h"

namespace hob::editor {
    class Editor;

    constexpr const char* DIRTY_MARKER = "(*)";

    std::optional<std::string> get_save_error(const Editor& editor);
    bool can_save(const Editor& editor);
    void save_all(Editor& editor);
    void revert_all(Editor& editor);

    bool can_new_scene(const Editor& editor);
    bool can_save_scene_as(const Editor& editor);
    void show_new_scene_dialog(Editor& editor);
    void show_save_scene_as_dialog(Editor& editor);

    std::optional<std::string> get_scene_create_error(const Editor& editor, const std::filesystem::path& path);
    void new_scene(Editor& editor, const std::filesystem::path& path);
    void save_scene_as(Editor& editor, const std::filesystem::path& path);

    bool can_new_prefab(const Editor& editor);
    void show_new_prefab_dialog(Editor& editor);

    std::optional<std::string> get_prefab_create_error(const Editor& editor, const std::filesystem::path& path);
    void new_prefab(Editor& editor, const std::filesystem::path& path);
    void create_prefab_from_entity(Editor& editor, EntityId entity_id, const std::filesystem::path& path);
    void create_prefab_from_entity_in_folder(Editor& editor, EntityId entity_id, const std::filesystem::path& folder);

    bool can_delete_selected_prefab(const Editor& editor);
    void request_delete_selected_prefab(Editor& editor);
    void request_delete_prefab(Editor& editor, const std::string& prefab_name);
    void delete_prefab(Editor& editor, const std::string& prefab_name);
} // namespace hob::editor
