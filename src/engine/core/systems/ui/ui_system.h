#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

#include "engine/core/aspect_mode.h"
#include "engine/math/vector2.h"
#include "ui_file_interface.h"
#include "ui_render_interface.h"
#include "ui_system_interface.h"

namespace Rml {
    class Context;
    class DataModelConstructor;
    class Element;
    class ElementDocument;
    class EventListener;
    class StyleSheetContainer;
} // namespace Rml

namespace hob {
    struct UiSystemConfig;
    class SdlContext;
    class Renderer;
    class Timer;

    using UiDocumentId = int64_t;
    constexpr UiDocumentId INVALID_UI_DOCUMENT_ID = -1;

    using UiElementId = int64_t;
    constexpr UiElementId INVALID_UI_ELEMENT_ID = -1;

    using UiListenerId = int64_t;
    constexpr UiListenerId INVALID_UI_LISTENER_ID = -1;

    using UiDataModelId = int64_t;
    constexpr UiDataModelId INVALID_UI_DATA_MODEL_ID = -1;

    // Scalar value stored in a data model field. Matches the Lua scalar types.
    using UiValue = std::variant<bool, int64_t, double, std::string>;

    struct UiDocument {
        Rml::ElementDocument* rml_document = nullptr;
        std::string path;
    };

    struct UiElement {
        UiDocumentId document_id = INVALID_UI_DOCUMENT_ID;
        std::string element_id;
        Rml::Element* rml_element = nullptr;
    };

    struct UiListener {
        UiDocumentId document_id = INVALID_UI_DOCUMENT_ID;
        std::string element_id;
        std::string event;
        std::unique_ptr<Rml::EventListener> listener;
    };

    struct UiDataModel {
        std::string name;
        std::unique_ptr<Rml::DataModelConstructor> constructor;
        std::unordered_map<std::string, UiValue> values;
    };

    class UiSystem {
        const SdlContext& m_sdl_context;
        const Renderer& m_renderer;

        UiFileInterface m_file_interface;
        UiSystemInterface m_system_interface;
        UiRenderInterface m_render_interface;

        Rml::Context* m_context = nullptr;
        std::shared_ptr<Rml::StyleSheetContainer> m_base_stylesheet;

        std::unordered_map<UiDocumentId, UiDocument> m_documents;
        UiDocumentId m_next_document_id = 0;

        std::unordered_map<UiElementId, UiElement> m_elements;
        UiElementId m_next_element_id = 0;

        std::unordered_map<UiListenerId, UiListener> m_listeners;
        UiListenerId m_next_listener_id = 0;

        std::unordered_map<UiDataModelId, UiDataModel> m_models;
        UiDataModelId m_next_model_id = 0;

        Vector2 m_reference_size;
        AspectMode m_aspect_mode;

        std::filesystem::file_time_type m_last_rml_write_time{};
        std::filesystem::file_time_type m_last_rcss_write_time{};
        bool m_has_asset_write_baseline = false;
        float m_asset_watch_accumulator = 0.0f;

    public:
        UiSystem(const UiSystemConfig& config, const SdlContext& sdl_context, Renderer& renderer, const Timer& timer);
        ~UiSystem();

        UiSystem(const UiSystem&) = delete;
        UiSystem& operator=(const UiSystem&) = delete;

        UiSystem(UiSystem&&) = delete;
        UiSystem& operator=(UiSystem&&) = delete;

        void process_event(const SDL_Event& event);

        void on_window_resized(int window_width, int window_height);

        void tick();

        void render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex);

        Vector2 screen_to_ui(const Vector2& screen_pos) const;

        UiDocumentId load_document(const std::string& relative_path);
        void unload_document(UiDocumentId id);
        void show_document(UiDocumentId id);
        void hide_document(UiDocumentId id);

        UiElementId get_element(UiDocumentId document_id, const std::string& element_id);

        std::string get_element_property(UiElementId id, const std::string& property);
        void set_element_property(UiElementId id, const std::string& property, const std::string& value);

        Vector2 get_element_position(UiElementId id);
        void set_element_position(UiElementId id, const Vector2& position);

        UiListenerId add_event_listener(UiElementId element_id,
                                        const std::string& event,
                                        std::function<void()> callback);
        void remove_event_listener(UiListenerId id);
        void clear_event_listeners();

        UiDataModelId create_model(const std::string& name, const std::unordered_map<std::string, UiValue>& fields);
        void destroy_model(UiDataModelId id);
        void clear_data_models();
        UiValue get_model_value(UiDataModelId id, const std::string& field);
        void set_model_value(UiDataModelId id, const std::string& field, UiValue value);
        void bind_model_event(UiDataModelId id, const std::string& event, std::function<void()> callback);

        void hot_reload_documents();
        void hot_reload_stylesheets();
        void poll_hot_reload(float delta_time);

    private:
        void detach_listener(const UiListener& record);

        UiDocument* find_document(UiDocumentId id);
        UiElement* find_element(UiElementId id);
        UiDataModel* find_model(UiDataModelId id);

        Rml::ElementDocument* instantiate_document(const std::string& relative_path);
        void apply_base_stylesheet(Rml::ElementDocument& document) const;
    };
} // namespace hob
