#include "ui_system.h"

#include <filesystem>

#include <RmlUi/Core.h>

#include "engine/core/debug.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/renderer/renderer.h"
#include "engine/core/systems/sdl_context.h"

namespace hob {
    namespace {
        constexpr std::string_view UI_FONT_PATH = "assets/builtin/fonts/jetbrains_mono_bold.ttf";
        constexpr const char* UI_CONTEXT_NAME = "main";
    } // namespace

    UiSystem::UiSystem(const SdlContext& sdl_context, Renderer& renderer, const Timer& timer)
        : m_system_interface(timer)
        , m_render_interface(sdl_context, renderer) {

        Rml::SetSystemInterface(&m_system_interface);
        Rml::SetRenderInterface(&m_render_interface);

        if (!Rml::Initialise()) {
            debug::log_error("UiSystem init failed: Rml::Initialise() returned false");
            return;
        }

        debug::log("Rml::Initialise (RmlUi {})", Rml::GetVersion());

        const std::filesystem::path font_path = PathUtils::get_root_path() / std::filesystem::path(UI_FONT_PATH);
        if (!Rml::LoadFontFace(font_path.string())) {
            debug::log_error("UiSystem init failed: could not load font '{}'", font_path.string());
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

        m_context->Render();
    }
} // namespace hob
