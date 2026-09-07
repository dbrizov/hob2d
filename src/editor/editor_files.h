#pragma once

#include <filesystem>
#include <optional>
#include <string>

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
    bool can_create_prefab_from_selection(const Editor& editor);
    void show_new_prefab_dialog(Editor& editor);
    void show_create_prefab_from_selection_dialog(Editor& editor);

    std::optional<std::string> get_prefab_create_error(const Editor& editor, const std::filesystem::path& path);
    void new_prefab(Editor& editor, const std::filesystem::path& path);
    void create_prefab_from_selection(Editor& editor, const std::filesystem::path& path);
} // namespace hob::editor
