#include "editor_dock_inspector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <sol/sol.hpp>

#include "editor/commands/editor_command_set_asset_field.h"
#include "editor/commands/editor_command_set_field.h"
#include "editor/editor.h"
#include "editor/editor_asset_value.h"
#include "editor/editor_definition.h"
#include "editor/editor_field_target.h"
#include "editor/editor_files.h"
#include "editor/editor_gui_utils.h"
#include "editor/editor_lua.h"
#include "engine/core/engine.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/entity_spawner.h"
#include "engine/core/systems/scripting/lua_schema_keys.h"
#include "engine/core/systems/scripting/lua_script_system.h"
#include "engine/entity/entity.h"
#include "engine/math/aabb.h"
#include "engine/math/capsule.h"
#include "engine/math/circle.h"
#include "engine/math/color.h"
#include "engine/math/constants.h"
#include "engine/math/vector2.h"

namespace hob::editor {
    struct EditorDockInspectorPendingEdit {
        EditorFieldTarget target;
        sol::object old_value;
        bool active = false;
    };

    namespace {
        template<typename T>
        T value_or(const sol::object& value, const T& fallback) {
            return value.is<T>() ? value.as<T>() : fallback;
        }

        int64_t to_int_bound(float value) {
            if (value >= MAX_FLOAT) {
                return MAX_INT64;
            }

            if (value <= MIN_FLOAT) {
                return MIN_INT64;
            }

            return static_cast<int64_t>(value);
        }

        void begin_pending_edit(EditorDockInspectorPendingEdit& pending,
                                const EditorFieldTarget& target,
                                const sol::object& old_value) {
            pending.target = target;
            pending.old_value = old_value;
            pending.active = true;
        }

        void clear_pending_edit(EditorDockInspectorPendingEdit& pending) {
            pending = EditorDockInspectorPendingEdit{};
        }

        void commit_field_edit(Editor& editor,
                               EditorDockInspectorPendingEdit& pending,
                               const EditorFieldTarget& target,
                               const std::string& command_label,
                               const sol::object& old_value,
                               const sol::object& new_value,
                               bool changed) {
            if (changed) {
                if (!pending.active || !(pending.target == target)) {
                    begin_pending_edit(pending, target, old_value);
                }

                EditorCommandSetField::apply(editor, target, new_value);
            }

            if (!pending.active || !(pending.target == target)) {
                return;
            }

            // Drags, checkboxes and text input all report their release. A pick inside field_color's
            // popup does not -- its items live in another window, so the row's own group never sees the
            // deactivation -- hence the second condition: nothing anywhere is being dragged any more.
            if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive()) {
                const sol::object final_value = changed ? new_value : old_value;
                editor.get_commands().push(editor,
                                           std::make_unique<EditorCommandSetField>(
                                               command_label, pending.target, pending.old_value, final_value));
                clear_pending_edit(pending);
            }
        }

        void draw_field(Editor& editor,
                        EditorDockInspectorPendingEdit& pending,
                        const EditorFieldTarget& component_target,
                        const std::string& component_label,
                        const sol::table& field) {
            const std::string name = field.get_or<std::string>(query_key::NAME, "?");
            const std::string label = to_display_label(name);
            const std::string type = field.get_or<std::string>(query_key::TYPE, "");
            const sol::object value = field[query_key::VALUE];

            // min == max means unbounded.
            const std::string enum_name = field.get_or<std::string>(component_schema_key::ENUM, "");
            const float min = field.get_or(component_schema_key::MIN, 0.0f);
            const float max = field.get_or(component_schema_key::MAX, 0.0f);

            Engine& engine = editor.get_engine();
            sol::state& lua = engine.get_lua_script_system().get_lua();
            sol::object new_value;
            bool changed = false;

            if (type == field_type::INT) {
                int64_t number = value_or<int64_t>(value, 0);
                if (field_int(label.c_str(), number, INSPECTOR_DRAG_SPEED_INT, to_int_bound(min), to_int_bound(max))) {
                    new_value = sol::make_object(lua, number);
                    changed = true;
                }
            }
            else if (type == field_type::FLOAT) {
                float number = value_or<float>(value, 0.0f);
                if (field_float(label.c_str(), number, INSPECTOR_DRAG_SPEED_FLOAT, min, max)) {
                    new_value = sol::make_object(lua, number);
                    changed = true;
                }
            }
            else if (type == field_type::ANGLE) {
                float degrees = value_or<float>(value, 0.0f) * RAD_TO_DEG;
                if (field_angle(label.c_str(), degrees, INSPECTOR_DRAG_SPEED_ROTATION_DEG)) {
                    new_value = sol::make_object(lua, degrees * DEG_TO_RAD);
                    changed = true;
                }
            }
            else if (type == field_type::BOOL) {
                bool flag = value_or<bool>(value, false);
                if (field_bool(label.c_str(), flag)) {
                    new_value = sol::make_object(lua, flag);
                    changed = true;
                }
            }
            else if (type == field_type::STRING) {
                std::string text = value_or<std::string>(value, "");
                if (field_string(label.c_str(), text)) {
                    new_value = sol::make_object(lua, text);
                    changed = true;
                }
            }
            else if (type == field_type::VECTOR2) {
                Vector2 vector = value_or<Vector2>(value, Vector2());
                if (field_vector2(label.c_str(), vector)) {
                    new_value = sol::make_object(lua, vector);
                    changed = true;
                }
            }
            else if (type == field_type::COLOR) {
                Color color = value_or<Color>(value, Color());
                if (field_color(label.c_str(), color)) {
                    new_value = sol::make_object(lua, color);
                    changed = true;
                }
            }
            else if (type == field_type::AABB) {
                AABB box = value_or<AABB>(value, AABB());
                if (field_aabb(label.c_str(), box)) {
                    new_value = sol::make_object(lua, box);
                    changed = true;
                }
            }
            else if (type == field_type::CAPSULE) {
                Capsule capsule = value_or<Capsule>(value, Capsule());
                if (field_capsule(label.c_str(), capsule)) {
                    new_value = sol::make_object(lua, capsule);
                    changed = true;
                }
            }
            else if (type == field_type::CIRCLE) {
                Circle circle = value_or<Circle>(value, Circle());
                if (field_circle(label.c_str(), circle)) {
                    new_value = sol::make_object(lua, circle);
                    changed = true;
                }
            }
            else if (type == field_type::ENUM || type == field_type::BITMASK) {
                // Discrete: a pick or a flag toggle is the whole edit, so it skips the drag
                // coalescing and pushes its command the frame it happens.
                const std::vector<EditorInspectorEntryEnum> entries = get_enum_entries(engine, enum_name);
                int64_t flags = value_or<int64_t>(value, 0);

                const bool edited = type == field_type::ENUM ? field_enum(label.c_str(), flags, entries)
                                                             : field_bitmask(label.c_str(), flags, entries);
                if (edited) {
                    EditorFieldTarget discrete_target = component_target;
                    discrete_target.field = name;

                    editor.get_commands().push(
                        editor,
                        std::make_unique<EditorCommandSetField>("Set " + component_label + " " + label,
                                                                discrete_target,
                                                                value,
                                                                sol::make_object(lua, flags)));
                }

                return;
            }
            else if (const char* factory_name = get_asset_factory_name_for_field_type(type); factory_name != nullptr) {
                const std::vector<EditorInspectorEntryAsset>& entries = get_asset_entries(engine, factory_name);
                const std::string asset_name = get_asset_name(engine, factory_name, value);
                const std::string display_name = lua_object_to_display_string(engine, value);

                std::string picked_asset_name;
                if (field_asset(
                        label.c_str(), asset_name, display_name, is_asset_set(value), entries, picked_asset_name)) {
                    EditorFieldTarget asset_target = component_target;
                    asset_target.field = name;

                    EditorAssetValue old_asset;
                    old_asset.asset_name = asset_name;
                    if (asset_name.empty() && is_asset_set(value)) {
                        old_asset.inline_asset = value;
                    }

                    EditorAssetValue new_asset;
                    new_asset.asset_name = picked_asset_name;

                    editor.get_commands().push(
                        editor,
                        std::make_unique<EditorCommandSetAssetField>("Set " + component_label + " " + label,
                                                                     asset_target,
                                                                     factory_name,
                                                                     std::move(old_asset),
                                                                     std::move(new_asset)));
                }

                return;
            }
            else {
                field_text(label.c_str(), lua_object_to_display_string(engine, value));
                return;
            }

            EditorFieldTarget target = component_target;
            target.field = name;

            commit_field_edit(
                editor, pending, target, "Set " + component_label + " " + label, value, new_value, changed);
        }

        void draw_component(Editor& editor,
                            EditorDockInspectorPendingEdit& pending,
                            const EditorFieldTarget& owner,
                            int32_t index,
                            const sol::table& component) {
            const std::string name = component.get_or<std::string>(query_key::NAME, "?");
            const std::string label = to_display_label(name);
            const bool is_lua = component.get_or(query_key::IS_LUA, false);
            const std::string header = is_lua ? label + " (Lua)" : label;

            EditorFieldTarget target = owner;
            target.is_lua = is_lua;
            target.component_key = name;

            ImGui::PushID(index);

            if (component_header(header.c_str())) {
                const sol::object fields = component[query_key::FIELDS];
                if (fields.is<sol::table>()) {
                    const sol::table rows = fields.as<sol::table>();
                    for (int32_t i = 1; i <= rows.size(); ++i) {
                        const sol::object row = rows[i];
                        if (row.is<sol::table>()) {
                            draw_field(editor, pending, target, label, row.as<sol::table>());
                        }
                    }
                }
            }

            ImGui::PopID();
        }

        void clear_pending_edit_of_other_owner(EditorDockInspectorPendingEdit& pending,
                                               const EditorFieldTarget& owner) {
            if (pending.active && !pending.target.has_same_owner(owner)) {
                clear_pending_edit(pending);
            }
        }

        void draw_sections(Editor& editor,
                           EditorDockInspectorPendingEdit& pending,
                           const EditorFieldTarget& owner,
                           const sol::table& sections) {
            for (int32_t i = 1; i <= sections.size(); ++i) {
                const sol::object section = sections[i];
                if (section.is<sol::table>()) {
                    draw_component(editor, pending, owner, i, section.as<sol::table>());
                }
            }
        }

        void draw_prefab_link(Editor& editor, const std::string& prefab_name) {
            ImGui::TextDisabled("Prefab:");
            ImGui::SameLine();
            if (ImGui::TextLink(prefab_name.c_str())) {
                editor.get_selection().select_definition({.registry = def_registry::ENTITIES, .name = prefab_name});
            }
        }

        void draw_entity(Editor& editor, EditorDockInspectorPendingEdit& pending, const Entity& entity) {
            EditorFieldTarget owner;
            owner.entity_id = entity.get_id();

            clear_pending_edit_of_other_owner(pending, owner);

            ImGui::Text("%s", entity.get_display_name().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("#%lld", static_cast<long long>(entity.get_id()));

            if (!entity.get_prefab_name().empty()) {
                draw_prefab_link(editor, entity.get_prefab_name());
            }

            const size_t selected_count = editor.get_selection().ids.size();
            if (selected_count > 1) {
                ImGui::TextDisabled("(%zu selected)", selected_count);
            }

            Engine& engine = editor.get_engine();

            const sol::object components = editor_call(engine, editor_func::GET_COMPONENTS, entity.get_id());
            if (!components.is<sol::table>()) {
                ImGui::TextDisabled("Editor.get_components is unavailable");
                return;
            }

            draw_sections(editor, pending, owner, components.as<sol::table>());
        }

        bool is_prefab_dirty(Editor& editor, const std::string& prefab_name) {
            const sol::object result = editor_call(editor.get_engine(), editor_func::IS_PREFAB_DIRTY, prefab_name);
            return result.is<bool>() && result.as<bool>();
        }

        bool can_edit_prefab_document(Editor& editor, const EditorDefinitionRef& ref) {
            if (ref.registry != def_registry::ENTITIES || editor.get_state() != WorldState::Stopped) {
                return false;
            }

            const EditorDefinition* definition = find_definition(editor.get_engine(), ref);
            return definition != nullptr && !definition->read_only;
        }

        void draw_definition(Editor& editor, EditorDockInspectorPendingEdit& pending, const EditorDefinitionRef& ref) {
            EditorFieldTarget owner;
            if (ref.registry == def_registry::ENTITIES) {
                owner.prefab_name = ref.name;
            }

            clear_pending_edit_of_other_owner(pending, owner);

            Engine& engine = editor.get_engine();

            ImGui::Text("%s", ref.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ref.registry.c_str());
            if (owner.is_prefab_document() && is_prefab_dirty(editor, ref.name)) {
                ImGui::SameLine();
                ImGui::TextUnformatted(DIRTY_MARKER);
            }

            const EditorDefinition* definition = find_definition(engine, ref);
            if (definition != nullptr && !definition->file.empty()) {
                ImGui::TextDisabled("%s",
                                    PathUtils::to_project_relative_path(definition->file).generic_string().c_str());
            }

            const sol::object result =
                editor_call(engine, editor_func::GET_DEFINITION_SECTIONS, ref.registry, ref.name);
            if (!result.is<sol::table>()) {
                ImGui::TextDisabled("Editor.get_definition_sections is unavailable");
                return;
            }

            const bool editable = can_edit_prefab_document(editor, ref);
            if (!editable) {
                ImGui::BeginDisabled();
            }

            draw_sections(editor, pending, owner, result.as<sol::table>());

            if (!editable) {
                ImGui::EndDisabled();
            }
        }
    } // namespace

    EditorDockInspector::EditorDockInspector()
        : EditorDock("Inspector", EditorActionContext::Inspector)
        , m_pending(std::make_unique<EditorDockInspectorPendingEdit>()) {}

    EditorDockInspector::~EditorDockInspector() = default;

    void EditorDockInspector::draw(Editor& editor) {
        if (begin()) {
            const EditorSelection& selection = editor.get_selection();

            // Multi-selection inspects the primary; the rest still move together via the SceneView.
            Entity* entity = editor.get_engine().get_entity_spawner().get_entity(selection.primary());
            if (selection.definition.is_valid()) {
                draw_definition(editor, *m_pending, selection.definition);
            }
            else if (entity == nullptr) {
                ImGui::TextDisabled("Select an entity or an asset");
            }
            else {
                draw_entity(editor, *m_pending, *entity);
            }
        }
        end();
    }

    void EditorDockInspector::reset_edit_state() {
        clear_pending_edit(*m_pending);
    }
} // namespace hob::editor
