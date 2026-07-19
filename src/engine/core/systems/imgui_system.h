#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

struct ImGuiContext;

namespace hob {
    class SdlContext;

    class ImGuiSystem {
        static constexpr float DEFAULT_FONT_SIZE_PX = 20.0f;

        ImGuiContext* m_context = nullptr;
        SDL_GPUDevice* m_gpu_device = nullptr;

    public:
        explicit ImGuiSystem(const SdlContext& sdl_context);
        ~ImGuiSystem();

        ImGuiSystem(const ImGuiSystem&) = delete;
        ImGuiSystem& operator=(const ImGuiSystem&) = delete;

        ImGuiSystem(ImGuiSystem&&) = delete;
        ImGuiSystem& operator=(ImGuiSystem&&) = delete;

        void process_event(const SDL_Event& event);

        void new_frame();
        void render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex);
        void discard_frame();
    };
} // namespace hob
