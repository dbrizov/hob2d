#include "ui_system.h"

#include <RmlUi/Core.h>

#include "engine/core/debug.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    namespace {
        constexpr const char* UI_FONT_PATH = "builtin/fonts/jetbrains_mono_bold.ttf";
        constexpr const char* UI_TEST_DOCUMENT = "ui/test.rml";
        constexpr const char* UI_CONTEXT_NAME = "main";
    } // namespace

    UiSystem::UiSystem(const SdlContext& sdl_context, Renderer& renderer, const Timer& timer)
        : m_system_interface(timer)
        , m_render_interface(sdl_context, renderer) {

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

        const Vector2 logical_size = renderer.get_logical_size();
        const Rml::Vector2i dimensions(static_cast<int>(logical_size.x), static_cast<int>(logical_size.y));

        m_context = Rml::CreateContext(UI_CONTEXT_NAME, dimensions);
        if (m_context == nullptr) {
            debug::log_error("UiSystem init failed: Rml::CreateContext() returned null");
            Rml::Shutdown();
            return;
        }

        debug::log("Rml::CreateContext('{}', {}x{})", UI_CONTEXT_NAME, dimensions.x, dimensions.y);

        Rml::ElementDocument* document = m_context->LoadDocument(UI_TEST_DOCUMENT);
        if (document == nullptr) {
            debug::log_error("UiSystem: could not load document '{}'", UI_TEST_DOCUMENT);
        }
        else {
            document->Show();
            debug::log("Rml::LoadDocument('{}')", UI_TEST_DOCUMENT);
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

    void UiSystem::process_event(const SDL_Event& event) {}

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
} // namespace hob
