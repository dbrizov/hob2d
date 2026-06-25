#include "ui_system.h"

#include <filesystem>
#include <memory>

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include "engine/core/assert.h"
#include "engine/core/debug.h"
#include "engine/core/engine_config.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    namespace {
        constexpr const char* UI_FONT_PATH = "builtin/fonts/jetbrains_mono_bold.ttf";
        constexpr const char* UI_BASE_STYLESHEET = "builtin/ui/base.rcss";
        constexpr const char* UI_CONTEXT_NAME = "main";

        class CallbackListener : public Rml::EventListener {
            std::function<void()> m_callback;

        public:
            explicit CallbackListener(std::function<void()> callback)
                : m_callback(std::move(callback)) {}

            void ProcessEvent(Rml::Event& event) override {
                if (m_callback) {
                    m_callback();
                }
            }
        };

        int to_rml_key_modifiers() {
            const SDL_Keymod mod = SDL_GetModState();
            int result = 0;
            if (mod & SDL_KMOD_CTRL) {
                result |= Rml::Input::KM_CTRL;
            }
            if (mod & SDL_KMOD_SHIFT) {
                result |= Rml::Input::KM_SHIFT;
            }
            if (mod & SDL_KMOD_ALT) {
                result |= Rml::Input::KM_ALT;
            }
            if (mod & SDL_KMOD_GUI) {
                result |= Rml::Input::KM_META;
            }
            if (mod & SDL_KMOD_CAPS) {
                result |= Rml::Input::KM_CAPSLOCK;
            }
            if (mod & SDL_KMOD_NUM) {
                result |= Rml::Input::KM_NUMLOCK;
            }
            return result;
        }

        int to_rml_mouse_button(uint8_t sdl_button) {
            switch (sdl_button) {
                case SDL_BUTTON_LEFT:
                    return 0;
                case SDL_BUTTON_RIGHT:
                    return 1;
                case SDL_BUTTON_MIDDLE:
                    return 2;
                default:
                    return -1;
            }
        }

        Rml::Variant to_rml_variant(const UiValue& value) {
            switch (value.index()) {
                case 0:
                    return Rml::Variant(std::get<bool>(value));
                case 1:
                    return Rml::Variant(std::get<int64_t>(value));
                case 2:
                    return Rml::Variant(std::get<double>(value));
                default:
                    return Rml::Variant(std::get<std::string>(value));
            }
        }

        UiValue from_rml_variant(const Rml::Variant& variant) {
            switch (variant.GetType()) {
                case Rml::Variant::BOOL:
                    return variant.Get<bool>();
                case Rml::Variant::INT:
                    return static_cast<int64_t>(variant.Get<int>());
                case Rml::Variant::INT64:
                    return variant.Get<int64_t>();
                case Rml::Variant::FLOAT:
                    return static_cast<double>(variant.Get<float>());
                case Rml::Variant::DOUBLE:
                    return variant.Get<double>();
                default:
                    return variant.Get<Rml::String>();
            }
        }
    } // namespace

    UiSystem::UiSystem(const UiSystemConfig& config,
                       const SdlContext& sdl_context,
                       Renderer& renderer,
                       const Timer& timer)
        : m_sdl_context(sdl_context)
        , m_renderer(renderer)
        , m_system_interface(timer)
        , m_render_interface(sdl_context, renderer)
        , m_reference_size(static_cast<float>(config.reference_width), static_cast<float>(config.reference_height))
        , m_aspect_mode(config.aspect_mode) {

        m_render_interface.init();

        Rml::SetFileInterface(&m_file_interface);
        Rml::SetSystemInterface(&m_system_interface);
        Rml::SetRenderInterface(&m_render_interface);

        const bool rml_initialized = Rml::Initialise();
        HOB_CHECK(rml_initialized, "UiSystem init failed: Rml::Initialise() returned false");

        log::ui.info("Rml::Initialise (RmlUi {})", Rml::GetVersion());

        const bool font_loaded = Rml::LoadFontFace(UI_FONT_PATH);
        HOB_CHECK(font_loaded, "UiSystem init failed: could not load font '{}'", UI_FONT_PATH);

        m_base_stylesheet = Rml::Factory::InstanceStyleSheetFile(UI_BASE_STYLESHEET);
        if (!m_base_stylesheet) {
            log::ui.error("UiSystem: could not load base stylesheet '{}'", UI_BASE_STYLESHEET);
        }

        const Rml::Vector2i dimensions(static_cast<int>(m_reference_size.x), static_cast<int>(m_reference_size.y));

        m_context = Rml::CreateContext(UI_CONTEXT_NAME, dimensions);
        HOB_CHECK(m_context, "UiSystem init failed: Rml::CreateContext() returned null");

        log::ui.info("Rml::CreateContext('{}', {}x{})", UI_CONTEXT_NAME, dimensions.x, dimensions.y);

        int window_width = 0;
        int window_height = 0;
        m_sdl_context.get_window_size_px(window_width, window_height);
        on_window_resized(window_width, window_height);
    }

    UiSystem::~UiSystem() {
        Rml::RemoveContext(UI_CONTEXT_NAME);
        m_context = nullptr;
        log::ui.info("Rml::RemoveContext('{}')", UI_CONTEXT_NAME);

        m_base_stylesheet.reset();

        Rml::Shutdown();
        log::ui.info("Rml::Shutdown");
    }

    void UiSystem::process_event(const SDL_Event& event) {
        if (m_context == nullptr) {
            return;
        }

        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION: {
                const Vector2 window_size = m_sdl_context.get_window_size();
                const Vector2 logical_size = m_render_interface.get_logical_size();
                const float sx = (window_size.x > 0.0f) ? logical_size.x / window_size.x : 1.0f;
                const float sy = (window_size.y > 0.0f) ? logical_size.y / window_size.y : 1.0f;
                m_context->ProcessMouseMove(static_cast<int>(event.motion.x * sx),
                                            static_cast<int>(event.motion.y * sy),
                                            to_rml_key_modifiers());
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                const int button = to_rml_mouse_button(event.button.button);
                if (button >= 0) {
                    m_context->ProcessMouseButtonDown(button, to_rml_key_modifiers());
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const int button = to_rml_mouse_button(event.button.button);
                if (button >= 0) {
                    m_context->ProcessMouseButtonUp(button, to_rml_key_modifiers());
                }
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL:
                m_context->ProcessMouseWheel(-event.wheel.y, to_rml_key_modifiers());
                break;
            default:
                break;
        }
    }

    void UiSystem::on_window_resized(int window_width, int window_height) {
        if (m_context == nullptr) {
            return;
        }

        const Vector2 logical_size = compute_logical_size(window_width, window_height, m_reference_size, m_aspect_mode);
        m_render_interface.set_logical_size(logical_size);
        m_context->SetDimensions(Rml::Vector2i(static_cast<int>(logical_size.x), static_cast<int>(logical_size.y)));
    }

    void UiSystem::tick() {
        if (m_context == nullptr) {
            return;
        }

        m_context->Update();
    }

    void UiSystem::render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex) {
        if (m_context == nullptr) {
            return;
        }

        m_render_interface.begin_frame(cmd, swap_tex);
        m_context->Render();
        m_render_interface.end_frame();
    }

    Vector2 UiSystem::screen_to_ui(const Vector2& screen_pos) const {
        const Vector2 renderer_size = m_renderer.get_logical_size();
        const Vector2 ui_size = m_render_interface.get_logical_size();
        const float x = (renderer_size.x > 0.0f) ? ((screen_pos.x / renderer_size.x) * ui_size.x) : screen_pos.x;
        const float y = (renderer_size.y > 0.0f) ? ((screen_pos.y / renderer_size.y) * ui_size.y) : screen_pos.y;

        return Vector2(x, y);
    }

    UiDocumentId UiSystem::load_document(const std::string& path) {
        Rml::ElementDocument* document = instantiate_document(path);
        if (document == nullptr) {
            return INVALID_UI_DOCUMENT_ID;
        }

        const UiDocumentId id = m_next_document_id++;
        m_documents.emplace(id, UiDocument{document, path});
        return id;
    }

    void UiSystem::unload_document(UiDocumentId id) {
        const auto it = m_documents.find(id);
        if (it == m_documents.end()) {
            log::ui.error("UiSystem::unload_document: invalid document {}", id);
            return;
        }

        // Unload first, then drop our records: unloading touches the listeners, and our records
        // own them - freeing them earlier would leave RmlUi using freed memory.
        m_context->UnloadDocument(it->second.rml_document);

        for (auto l_it = m_listeners.begin(); l_it != m_listeners.end();) {
            l_it = (l_it->second.document_id == id) ? m_listeners.erase(l_it) : std::next(l_it);
        }

        for (auto e_it = m_elements.begin(); e_it != m_elements.end();) {
            e_it = (e_it->second.document_id == id) ? m_elements.erase(e_it) : std::next(e_it);
        }

        m_documents.erase(it);
    }

    void UiSystem::show_document(UiDocumentId id) {
        if (const UiDocument* document = find_document(id)) {
            document->rml_document->Show();
        }
    }

    void UiSystem::hide_document(UiDocumentId id) {
        if (const UiDocument* document = find_document(id)) {
            document->rml_document->Hide();
        }
    }

    UiElementId UiSystem::get_element(UiDocumentId document_id, const std::string& element_id) {
        UiDocument* document = find_document(document_id);
        if (document == nullptr) {
            log::ui.error("UiSystem::get_element: invalid document {}", document_id);
            return INVALID_UI_ELEMENT_ID;
        }

        Rml::Element* element = document->rml_document->GetElementById(element_id);
        if (element == nullptr) {
            log::ui.error("UiSystem::get_element: no element '{}' in document {}", element_id, document_id);
            return INVALID_UI_ELEMENT_ID;
        }

        const UiElementId id = m_next_element_id++;
        m_elements.emplace(id, UiElement{document_id, element_id, element});
        return id;
    }

    std::string UiSystem::get_element_property(UiElementId id, const std::string& property) {
        UiElement* element = find_element(id);
        if (element == nullptr) {
            log::ui.error("UiSystem::get_element_property: invalid element {}", id);
            return {};
        }

        const Rml::Property* prop = element->rml_element->GetProperty(property);
        return (prop != nullptr) ? prop->ToString() : std::string{};
    }

    void UiSystem::set_element_property(UiElementId id, const std::string& property, const std::string& value) {
        UiElement* element = find_element(id);
        if (element == nullptr) {
            log::ui.error("UiSystem::set_element_property: invalid element {}", id);
            return;
        }

        element->rml_element->SetProperty(property, value);
    }

    Vector2 UiSystem::get_element_position(UiElementId id) {
        const UiElement* element = find_element(id);
        if (element == nullptr) {
            log::ui.error("UiSystem::get_element_position: invalid element {}", id);
            return Vector2();
        }

        const Rml::Property* left = element->rml_element->GetProperty(Rml::PropertyId::Left);
        const Rml::Property* top = element->rml_element->GetProperty(Rml::PropertyId::Top);
        const float x = (left != nullptr) ? left->Get<float>() : 0.0f;
        const float y = (top != nullptr) ? top->Get<float>() : 0.0f;
        return Vector2(x, y);
    }

    void UiSystem::set_element_position(UiElementId id, const Vector2& position) {
        UiElement* element = find_element(id);
        if (element == nullptr) {
            log::ui.error("UiSystem::set_element_position: invalid element {}", id);
            return;
        }

        element->rml_element->SetProperty(Rml::PropertyId::Left, Rml::Property(position.x, Rml::Unit::PX));
        element->rml_element->SetProperty(Rml::PropertyId::Top, Rml::Property(position.y, Rml::Unit::PX));
    }

    UiListenerId UiSystem::add_event_listener(UiElementId element_id,
                                              const std::string& event,
                                              std::function<void()> callback) {
        UiElement* element = find_element(element_id);
        if (element == nullptr) {
            log::ui.error("UiSystem::add_event_listener: invalid element {}", element_id);
            return INVALID_UI_LISTENER_ID;
        }

        auto listener = std::make_unique<CallbackListener>(std::move(callback));
        element->rml_element->AddEventListener(event, listener.get());

        const UiListenerId id = m_next_listener_id++;
        m_listeners.emplace(id, UiListener{element->document_id, element->element_id, event, std::move(listener)});
        return id;
    }

    void UiSystem::remove_event_listener(UiListenerId id) {
        const auto it = m_listeners.find(id);
        if (it == m_listeners.end()) {
            log::ui.error("UiSystem::remove_event_listener: invalid listener {}", id);
            return;
        }

        detach_listener(it->second);
        m_listeners.erase(it);
    }

    void UiSystem::clear_event_listeners() {
        for (auto& [id, record] : m_listeners) {
            detach_listener(record);
        }

        m_listeners.clear();
    }

    UiDataModelId UiSystem::create_model(const std::string& name,
                                         const std::unordered_map<std::string, UiValue>& fields) {
        if (m_context == nullptr) {
            return INVALID_UI_DATA_MODEL_ID;
        }

        Rml::DataModelConstructor constructor = m_context->CreateDataModel(name);
        if (!constructor) {
            log::ui.error("UiSystem::create_model: could not create data model '{}'", name);
            return INVALID_UI_DATA_MODEL_ID;
        }

        const UiDataModelId id = m_next_model_id++;
        UiDataModel& model = m_models[id];
        model.name = name;
        model.values = fields;
        model.constructor = std::make_unique<Rml::DataModelConstructor>(constructor);

        // Bind each field through getter/setter into our C++-owned storage. The map nodes
        // are address-stable, so capturing the slot pointer is safe for the model's lifetime.
        for (auto& [field, value] : model.values) {
            UiValue* slot = &value;
            model.constructor->BindFunc(
                field,
                [slot](Rml::Variant& out) {
                    out = to_rml_variant(*slot);
                },
                [slot](const Rml::Variant& in) {
                    *slot = from_rml_variant(in);
                });
        }

        return id;
    }

    void UiSystem::destroy_model(UiDataModelId id) {
        const auto it = m_models.find(id);
        if (it == m_models.end()) {
            log::ui.error("UiSystem::destroy_model: invalid model {}", id);
            return;
        }

        if (m_context != nullptr) {
            m_context->RemoveDataModel(it->second.name);
        }

        m_models.erase(it);
    }

    void UiSystem::clear_data_models() {
        if (m_context != nullptr) {
            for (auto& [id, model] : m_models) {
                m_context->RemoveDataModel(model.name);
            }
        }

        m_models.clear();
    }

    UiValue UiSystem::get_model_value(UiDataModelId id, const std::string& field) {
        UiDataModel* model = find_model(id);
        if (model == nullptr) {
            log::ui.error("UiSystem::get_model_value: invalid model {}", id);
            return UiValue{};
        }

        const auto it = model->values.find(field);
        if (it == model->values.end()) {
            log::ui.error("UiSystem::get_model_value: model {} has no field '{}'", id, field);
            return UiValue{};
        }

        return it->second;
    }

    void UiSystem::set_model_value(UiDataModelId id, const std::string& field, UiValue value) {
        UiDataModel* model = find_model(id);
        if (model == nullptr) {
            log::ui.error("UiSystem::set_model_value: invalid model {}", id);
            return;
        }

        const auto it = model->values.find(field);
        if (it == model->values.end()) {
            log::ui.error("UiSystem::set_model_value: model {} has no field '{}'", id, field);
            return;
        }

        it->second = std::move(value);
        model->constructor->GetModelHandle().DirtyVariable(field);
    }

    void UiSystem::bind_model_event(UiDataModelId id, const std::string& event, std::function<void()> callback) {
        UiDataModel* model = find_model(id);
        if (model == nullptr) {
            log::ui.error("UiSystem::bind_model_event: invalid model {}", id);
            return;
        }

        model->constructor->BindEventCallback(
            event, [callback = std::move(callback)](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                callback();
            });
    }

    void UiSystem::detach_listener(const UiListener& record) {
        if (const UiDocument* document = find_document(record.document_id)) {
            if (Rml::Element* element = document->rml_document->GetElementById(record.element_id)) {
                element->RemoveEventListener(record.event, record.listener.get());
            }
        }
    }

    void UiSystem::hot_reload_documents() {
        Rml::Factory::ClearStyleSheetCache();

        for (auto& [doc_id, document] : m_documents) {
            Rml::ElementDocument* old_document = document.rml_document;

            // Load the new document before unloading the old one, so a syntax error
            // mid-edit leaves the existing document intact.
            Rml::ElementDocument* new_document = instantiate_document(document.path);
            if (new_document == nullptr) {
                continue;
            }

            if (old_document->IsVisible()) {
                new_document->Show();
            }

            m_context->UnloadDocument(old_document);
            document.rml_document = new_document;

            // Re-resolve element handles and re-attach listeners against the rebuilt tree
            // (the old Rml::Element* are now destroyed). Handle ids stay stable for Lua.
            for (auto& [id, ui_element] : m_elements) {
                if (ui_element.document_id == doc_id) {
                    ui_element.rml_element = new_document->GetElementById(ui_element.element_id);
                }
            }

            for (auto& [id, ui_listener] : m_listeners) {
                if (ui_listener.document_id != doc_id) {
                    continue;
                }

                if (Rml::Element* target = new_document->GetElementById(ui_listener.element_id)) {
                    target->AddEventListener(ui_listener.event, ui_listener.listener.get());
                }
                else {
                    log::ui.error("UiSystem: listener element '{}' missing after reload of '{}'",
                                  ui_listener.element_id,
                                  document.path);
                }
            }
        }

        debug::print(Color::white(), 5.0f, true, "[UI] RML hot reload");
    }

    void UiSystem::hot_reload_stylesheets() {
        Rml::Factory::ClearStyleSheetCache();

        if (auto base = Rml::Factory::InstanceStyleSheetFile(UI_BASE_STYLESHEET)) {
            m_base_stylesheet = std::move(base);
        }
        else {
            log::ui.error("UiSystem: could not reload base stylesheet '{}'", UI_BASE_STYLESHEET);
        }

        for (auto& [id, document] : m_documents) {
            document.rml_document->ReloadStyleSheet();
            apply_base_stylesheet(*document.rml_document);
        }

        debug::print(Color::white(), 5.0f, true, "[UI] RCSS hot reload");
    }

    void UiSystem::poll_hot_reload(float delta_time) {
        constexpr float POLL_INTERVAL = 0.5f;
        m_asset_watch_accumulator += delta_time;
        if (m_asset_watch_accumulator < POLL_INTERVAL) {
            return;
        }
        m_asset_watch_accumulator = 0.0f;

        const std::filesystem::path assets_root = PathUtils::get_assets_root_path();

        std::filesystem::file_time_type newest_rml = std::filesystem::file_time_type::min();
        std::filesystem::file_time_type newest_rcss = std::filesystem::file_time_type::min();
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assets_root, ec)) {
            if (ec) {
                return;
            }

            if (!entry.is_regular_file()) {
                continue;
            }

            const std::filesystem::path ext = entry.path().extension();
            std::filesystem::file_time_type* bucket = nullptr;
            if (ext == ".rml") {
                bucket = &newest_rml;
            }
            else if (ext == ".rcss") {
                bucket = &newest_rcss;
            }
            else {
                continue;
            }

            const std::filesystem::file_time_type t = entry.last_write_time(ec);
            if (!ec && t > *bucket) {
                *bucket = t;
            }
        }

        // First poll just records the baseline; never reload on startup.
        if (!m_has_asset_write_baseline) {
            m_last_rml_write_time = newest_rml;
            m_last_rcss_write_time = newest_rcss;
            m_has_asset_write_baseline = true;
            return;
        }

        // An .rml change triggers a full document reload, which re-reads linked .rcss too,
        // so absorb the .rcss baseline to avoid a redundant stylesheet reload next frame.
        if (newest_rml > m_last_rml_write_time) {
            m_last_rml_write_time = newest_rml;
            m_last_rcss_write_time = newest_rcss;
            hot_reload_documents();
        }
        else if (newest_rcss > m_last_rcss_write_time) {
            m_last_rcss_write_time = newest_rcss;
            hot_reload_stylesheets();
        }
    }

    UiDocument* UiSystem::find_document(UiDocumentId id) {
        const auto it = m_documents.find(id);
        return (it != m_documents.end()) ? &it->second : nullptr;
    }

    UiElement* UiSystem::find_element(UiElementId id) {
        const auto it = m_elements.find(id);
        return (it != m_elements.end()) ? &it->second : nullptr;
    }

    UiDataModel* UiSystem::find_model(UiDataModelId id) {
        const auto it = m_models.find(id);
        return (it != m_models.end()) ? &it->second : nullptr;
    }

    Rml::ElementDocument* UiSystem::instantiate_document(const std::string& path) {
        if (m_context == nullptr) {
            return nullptr;
        }

        Rml::ElementDocument* document = m_context->LoadDocument(path);
        if (document == nullptr) {
            log::ui.error("UiSystem::instantiate_document: could not load '{}'", path);
            return nullptr;
        }

        apply_base_stylesheet(*document);
        return document;
    }

    void UiSystem::apply_base_stylesheet(Rml::ElementDocument& document) const {
        if (!m_base_stylesheet) {
            return;
        }

        // Base first (lowest priority), the document's own styles merged on top.
        auto combined = Rml::MakeShared<Rml::StyleSheetContainer>();
        combined->MergeStyleSheetContainer(*m_base_stylesheet);
        if (const Rml::StyleSheetContainer* document_stylesheet = document.GetStyleSheetContainer()) {
            combined->MergeStyleSheetContainer(*document_stylesheet);
        }

        document.SetStyleSheetContainer(combined);
    }
} // namespace hob
