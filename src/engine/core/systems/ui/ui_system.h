#pragma once

#include <memory>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

#include "ui_file_interface.h"
#include "ui_render_interface.h"
#include "ui_system_interface.h"

namespace Rml {
    class Context;
    class EventListener;
    class ElementDocument;
    class StyleSheetContainer;
} // namespace Rml

namespace hob {
    class SdlContext;
    class Renderer;
    class Timer;

    class UiSystem {
        const SdlContext& m_sdl_context;
        const Renderer& m_renderer;

        bool m_is_initialized = false;

        UiFileInterface m_file_interface;
        UiSystemInterface m_system_interface;
        UiRenderInterface m_render_interface;

        Rml::Context* m_context = nullptr;
        std::unique_ptr<Rml::EventListener> m_click_listener;
        std::shared_ptr<Rml::StyleSheetContainer> m_base_stylesheet;

    public:
        UiSystem(const SdlContext& sdl_context, Renderer& renderer, const Timer& timer);
        ~UiSystem();

        UiSystem(const UiSystem&) = delete;
        UiSystem& operator=(const UiSystem&) = delete;

        UiSystem(UiSystem&&) = delete;
        UiSystem& operator=(UiSystem&&) = delete;

        bool is_initialized() const;

        void process_event(const SDL_Event& event);

        void tick();

        void render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex);

    private:
        void apply_base_stylesheet(Rml::ElementDocument& document) const;
    };
} // namespace hob
