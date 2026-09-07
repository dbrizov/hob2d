#include "editor_dock_assets.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>

#include "editor/editor.h"
#include "editor/editor_definition.h"
#include "editor/editor_files.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_lua.h"
#include "editor/editor_style.h"
#include "engine/core/engine.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"

namespace hob::editor {
    namespace {
        struct EditorFileNode {
            std::string label;
            std::string tooltip;
            std::string texture_name;
            EditorDefinitionRef definition;
            bool is_folder = false;
            bool read_only = false;
            std::vector<EditorFileNode> children;
        };

        using DefinitionsByFile = std::unordered_map<std::string, std::vector<const EditorDefinition*>>;

        std::string to_file_key(const std::filesystem::path& path) {
            return path.lexically_normal().generic_string();
        }

        DefinitionsByFile map_definitions_by_file(const std::vector<EditorDefinition>& definitions) {
            DefinitionsByFile by_file;
            for (const EditorDefinition& definition : definitions) {
                if (!definition.file.empty()) {
                    by_file[to_file_key(definition.file)].push_back(&definition);
                }
            }

            return by_file;
        }

        const std::vector<const EditorDefinition*>* find_definitions(const DefinitionsByFile& by_file,
                                                                     const std::filesystem::path& path) {
            const auto found = by_file.find(to_file_key(path));
            return found != by_file.end() ? &found->second : nullptr;
        }

        bool is_hidden_entry(const std::filesystem::path& path) {
            const std::string name = path.filename().string();
            return name.empty() || name.front() == '.';
        }

        bool sorts_before(const std::filesystem::path& a, const std::filesystem::path& b) {
            const std::string a_name = a.filename().string();
            const std::string b_name = b.filename().string();

            return std::lexicographical_compare(
                a_name.begin(), a_name.end(), b_name.begin(), b_name.end(), [](char left, char right) {
                    return std::tolower(static_cast<unsigned char>(left)) <
                           std::tolower(static_cast<unsigned char>(right));
                });
        }

        EditorFileNode build_file(const std::filesystem::path& path, const DefinitionsByFile& by_file) {
            EditorFileNode node;
            node.label = path.filename().string();
            node.tooltip = PathUtils::to_project_relative_path(path).generic_string();

            const std::vector<const EditorDefinition*>* declared = find_definitions(by_file, path);
            if (declared == nullptr) {
                declared = find_definitions(by_file, path.string() + file_extension::META);
            }

            if (declared == nullptr) {
                return node;
            }

            for (const EditorDefinition* definition : *declared) {
                node.tooltip += "\n" + definition->ref.registry + "." + definition->ref.name;
                node.read_only = node.read_only || definition->read_only;

                if (!node.definition.is_valid()) {
                    node.definition = definition->ref;
                }

                if (definition->ref.registry == def_registry::TEXTURES) {
                    node.texture_name = definition->ref.name;
                }
            }

            return node;
        }

        bool is_companion_of_present_file(const std::string& name, const std::unordered_set<std::string>& file_names) {
            if (!name.ends_with(file_extension::META)) {
                return false;
            }

            return file_names.contains(name.substr(0, name.size() - std::strlen(file_extension::META)));
        }

        EditorFileNode build_folder(const std::filesystem::path& folder, const DefinitionsByFile& by_file) {
            EditorFileNode node;
            node.label = folder.filename().string();
            node.tooltip = PathUtils::to_project_relative_path(folder).generic_string();
            node.is_folder = true;

            std::vector<std::filesystem::path> sub_folders;
            std::vector<std::filesystem::path> files;
            std::unordered_set<std::string> file_names;

            std::error_code error;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, error)) {
                if (is_hidden_entry(entry.path())) {
                    continue;
                }

                if (entry.is_directory()) {
                    sub_folders.push_back(entry.path());
                }
                else if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                    file_names.insert(entry.path().filename().string());
                }
            }

            std::sort(sub_folders.begin(), sub_folders.end(), sorts_before);
            std::sort(files.begin(), files.end(), sorts_before);

            for (const std::filesystem::path& sub_folder : sub_folders) {
                node.children.push_back(build_folder(sub_folder, by_file));
            }

            for (const std::filesystem::path& file : files) {
                if (!is_companion_of_present_file(file.filename().string(), file_names)) {
                    node.children.push_back(build_file(file, by_file));
                }
            }

            return node;
        }

        void draw_thumbnail(
            Editor& editor, const EditorFileNode& node, float gutter_x, const ImVec2& row_min, const ImVec2& row_max) {
            const TextureRef texture = get_texture(editor.get_engine(), node.texture_name);
            if (texture == nullptr) {
                return;
            }

            const float width = static_cast<float>(texture->get_width());
            const float height = static_cast<float>(texture->get_height());
            const float scale = ASSETS_THUMBNAIL_SIZE_PX / std::max({width, height, 1.0f});
            const ImVec2 size(width * scale, height * scale);

            const ImVec2 min(gutter_x + (ImGui::GetTreeNodeToLabelSpacing() - size.x) * 0.5f,
                             row_min.y + (row_max.y - row_min.y - size.y) * 0.5f);

            ImGui::GetWindowDrawList()->AddImage(
                texture->get_gpu_texture(), min, ImVec2(min.x + size.x, min.y + size.y));
        }

        using DirtyPrefabNames = std::unordered_set<std::string>;

        bool is_dirty_prefab(const EditorFileNode& node, const DirtyPrefabNames& dirty_prefabs) {
            return node.definition.registry == def_registry::ENTITIES && dirty_prefabs.contains(node.definition.name);
        }

        void draw_file(Editor& editor, const EditorFileNode& node, const DirtyPrefabNames& dirty_prefabs) {
            constexpr ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

            // A leaf leaves its arrow gutter empty, which is where the thumbnail goes -- so it has
            // to be measured before the row is submitted and painted after, over the row highlight.
            const float gutter_x = ImGui::GetCursorScreenPos().x;

            const bool is_selected = node.definition.is_valid() && editor.get_selection().definition == node.definition;

            tree_item(node.label.c_str(), flags, is_selected, "%s", node.label.c_str());

            if (node.definition.is_valid() && ImGui::IsItemClicked()) {
                editor.get_selection().select_definition(node.definition);
            }

            const ImVec2 row_min = ImGui::GetItemRectMin();
            const ImVec2 row_max = ImGui::GetItemRectMax();
            const bool is_row_visible = ImGui::IsItemVisible();

            if (ImGui::IsItemHovered()) {
                set_tooltip("%s", node.tooltip.c_str());
            }

            // Before the badge, which submits an item of its own for BeginDragDropSource to read instead.
            if (node.definition.registry == def_registry::ENTITIES && ImGui::BeginDragDropSource()) {
                set_drag_payload(DRAG_PAYLOAD_PREFAB, node.definition.name);
                ImGui::TextUnformatted(node.label.c_str());
                ImGui::EndDragDropSource();
            }

            if (is_dirty_prefab(node, dirty_prefabs)) {
                ImGui::SameLine(0.0f, ASSETS_BADGE_SPACING_PX);
                ImGui::TextUnformatted(DIRTY_MARKER);
            }

            if (node.read_only) {
                ImGui::SameLine(0.0f, ASSETS_BADGE_SPACING_PX);
                ImGui::TextColored(COLOR_ASSETS_READ_ONLY, "%s", ASSETS_READ_ONLY_LABEL);
            }

            // Building a texture to preview it reads the file and uploads it, so a row scrolled out
            // of view must not ask for one.
            if (!node.texture_name.empty() && is_row_visible) {
                draw_thumbnail(editor, node, gutter_x, row_min, row_max);
            }
        }

        void draw_node(Editor& editor,
                       const EditorFileNode& node,
                       int32_t depth,
                       const DirtyPrefabNames& dirty_prefabs) {
            if (!node.is_folder) {
                draw_file(editor, node, dirty_prefabs);
                return;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (depth <= ASSETS_DEFAULT_OPEN_DEPTH) {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }

            const bool open = tree_item(node.label.c_str(), flags, false, "%s", node.label.c_str());

            if (!open) {
                return;
            }

            for (const EditorFileNode& child : node.children) {
                draw_node(editor, child, depth + 1, dirty_prefabs);
            }

            ImGui::TreePop();
        }
    } // namespace

    struct EditorFileTree {
        EditorFileNode root;
        bool is_built = false;
    };

    EditorDockAssets::EditorDockAssets()
        : EditorDock("Assets", EditorActionContext::Assets)
        , m_tree(std::make_unique<EditorFileTree>()) {}

    EditorDockAssets::~EditorDockAssets() = default;

    void EditorDockAssets::draw(Editor& editor) {
        if (begin()) {
            if (!m_tree->is_built) {
                m_tree->root = build_folder(PathUtils::get_project_root(),
                                            map_definitions_by_file(get_definitions(editor.get_engine())));
                m_tree->is_built = true;
            }

            const std::vector<std::string> dirty_prefab_names = editor.get_dirty_prefab_names();
            const DirtyPrefabNames dirty_prefabs(dirty_prefab_names.begin(), dirty_prefab_names.end());

            EditorStyleVarStack vars;
            vars.push(ImGuiStyleVar_ItemSpacing, TREE_ITEM_SPACING);

            draw_node(editor, m_tree->root, 0, dirty_prefabs);

            vars.pop();
        }
        end();
    }

    void EditorDockAssets::request_rebuild() {
        m_tree->is_built = false;
    }
} // namespace hob::editor
