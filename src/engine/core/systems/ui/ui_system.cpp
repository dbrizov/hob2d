#include "ui_system.h"

#include <memory>

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include "engine/core/debug.h"
#include "engine/core/engine_config.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    namespace {
        constexpr const char* UI_FONT_PATH = "builtin/fonts/jetbrains_mono_bold.ttf";
        constexpr const char* UI_BASE_STYLESHEET = "builtin/ui/base.rcss";
        constexpr const char* UI_TEST_DOCUMENT = "ui/test.rml";
        constexpr const char* UI_CONTEXT_NAME = "main";

        class ClickLogger : public Rml::EventListener {
        public:
            void ProcessEvent(Rml::Event& event) override {
                debug::log("[UI] click: '{}'", event.GetCurrentElement()->GetId());
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
    } // namespace

    UiSystem::UiSystem(const UiSystemConfig& config,
                       const SdlContext& sdl_context,
                       Renderer& renderer,
                       const Timer& timer)
        : m_sdl_context(sdl_context)
        , m_renderer(renderer)
        , m_system_interface(timer)
        , m_render_interface(sdl_context, renderer)
        , m_reference_resolution(static_cast<float>(config.reference_width),
                                 static_cast<float>(config.reference_height))
        , m_screen_match_mode(config.screen_match_mode) {

        if (!m_render_interface.init()) {
            debug::log_error("UiSystem init failed: render interface init failed");
            return;
        }

        Rml::SetFileInterface(&m_file_interface);
        Rml::SetSystemInterface(&m_system_interface);
        Rml::SetRenderInterface(&m_render_interface);

        if (!Rml::Initialise()) {
            debug::log_error("UiSystem init failed: Rml::Initialise() returned false");
            return;
        }

        debug::log("Rml::Initialise (RmlUi {})", Rml::GetVersion());

        if (!Rml::LoadFontFace(UI_FONT_PATH)) {
            debug::log_error("UiSystem init failed: could not load font '{}'", UI_FONT_PATH);
            Rml::Shutdown();
            return;
        }

        debug::log("Rml::LoadFontFace('{}')", UI_FONT_PATH);

        m_base_stylesheet = Rml::Factory::InstanceStyleSheetFile(UI_BASE_STYLESHEET);
        if (!m_base_stylesheet) {
            debug::log_error("UiSystem: could not load base stylesheet '{}'", UI_BASE_STYLESHEET);
        }

        const Rml::Vector2i dimensions(static_cast<int>(m_reference_resolution.x),
                                       static_cast<int>(m_reference_resolution.y));

        m_context = Rml::CreateContext(UI_CONTEXT_NAME, dimensions);
        if (m_context == nullptr) {
            debug::log_error("UiSystem init failed: Rml::CreateContext() returned null");
            Rml::Shutdown();
            return;
        }

        debug::log("Rml::CreateContext('{}', {}x{})", UI_CONTEXT_NAME, dimensions.x, dimensions.y);

        update_logical_size();

        Rml::ElementDocument* document = m_context->LoadDocument(UI_TEST_DOCUMENT);
        if (document == nullptr) {
            debug::log_error("UiSystem: could not load document '{}'", UI_TEST_DOCUMENT);
        }
        else {
            apply_base_stylesheet(*document);
            document->Show();
            debug::log("Rml::LoadDocument('{}')", UI_TEST_DOCUMENT);

            Rml::Element* button = document->GetElementById("btn");
            if (button != nullptr) {
                m_click_listener = std::make_unique<ClickLogger>();
                button->AddEventListener("click", m_click_listener.get());
            }
        }

        m_is_initialized = true;
    }

    UiSystem::~UiSystem() {
        if (!m_is_initialized) {
            return;
        }

        Rml::Shutdown();
        debug::log("Rml::Shutdown");
    }

    bool UiSystem::is_initialized() const {
        return m_is_initialized;
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

    void UiSystem::tick() {
        if (m_context == nullptr) {
            return;
        }

        update_logical_size();
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

    Vector2 UiSystem::compute_effective_logical_size(int window_width, int window_height) const {
        if (window_width <= 0 || window_height <= 0) {
            return m_reference_resolution;
        }

        const float window_aspect = static_cast<float>(window_width) / static_cast<float>(window_height);
        const float reference_aspect = m_reference_resolution.x / m_reference_resolution.y;

        UiScreenMatchMode mode = m_screen_match_mode;
        if (mode == UiScreenMatchMode::expand) {
            mode =
                (window_aspect > reference_aspect) ? UiScreenMatchMode::match_height : UiScreenMatchMode::match_width;
        }
        else if (mode == UiScreenMatchMode::shrink) {
            mode =
                (window_aspect > reference_aspect) ? UiScreenMatchMode::match_width : UiScreenMatchMode::match_height;
        }

        if (mode == UiScreenMatchMode::match_width) {
            return Vector2(m_reference_resolution.x, m_reference_resolution.x / window_aspect);
        }

        return Vector2(m_reference_resolution.y * window_aspect, m_reference_resolution.y);
    }

    void UiSystem::update_logical_size() {
        int window_width = 0;
        int window_height = 0;
        m_sdl_context.get_window_size_px(window_width, window_height);
        if (window_width == m_last_window_width && window_height == m_last_window_height) {
            return;
        }

        m_last_window_width = window_width;
        m_last_window_height = window_height;

        const Vector2 logical_size = compute_effective_logical_size(window_width, window_height);
        m_render_interface.set_logical_size(logical_size);
        m_context->SetDimensions(Rml::Vector2i(static_cast<int>(logical_size.x), static_cast<int>(logical_size.y)));
    }
} // namespace hob
