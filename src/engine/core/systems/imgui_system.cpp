#include "imgui_system.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include "engine/core/logging.h"
#include "sdl_context.h"

namespace hob {
    ImGuiSystem::ImGuiSystem(const SdlContext& sdl_context) {
        SDL_Window* window = sdl_context.get_window();
        m_gpu_device = sdl_context.get_gpu_device();

        if (!window || !m_gpu_device) {
            log::imgui.error("ImGuiSystem init failed: window/GPU device is null");
            return;
        }

        IMGUI_CHECKVERSION();

        m_context = ImGui::CreateContext();
        if (!m_context) {
            log::imgui.error("ImGui_CreateContext failed");
            return;
        }

        log::imgui.info("ImGui_CreateContext()");

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImFontConfig font_cfg{};
        font_cfg.SizePixels = DEFAULT_FONT_SIZE_PX;
        io.Fonts->AddFontDefault(&font_cfg);

        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
            log::imgui.error("ImGui_ImplSDL3_InitForSDLGPU failed");
            ImGui::DestroyContext(m_context);
            m_context = nullptr;
            return;
        }

        log::imgui.info("ImGui_ImplSDL3_InitForSDLGPU");

        ImGui_ImplSDLGPU3_InitInfo init_info{};
        init_info.Device = m_gpu_device;
        init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(m_gpu_device, window);
        init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;

        if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
            log::imgui.error("ImGui_ImplSDLGPU3_Init failed");
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(m_context);
            m_context = nullptr;
            return;
        }

        log::imgui.info("ImGui_ImplSDLGPU3_Init");

        m_is_initialized = true;
    }

    ImGuiSystem::~ImGuiSystem() {
        if (!m_is_initialized) {
            return;
        }

        ImGui_ImplSDLGPU3_Shutdown();
        log::imgui.info("ImGui_ImplSDLGPU3_Shutdown");

        ImGui_ImplSDL3_Shutdown();
        log::imgui.info("ImGui_ImplSDL3_Shutdown");

        ImGui::DestroyContext(m_context);
        m_context = nullptr;
        log::imgui.info("ImGui_DestroyContext");
    }

    bool ImGuiSystem::is_initialized() const {
        return m_is_initialized;
    }

    void ImGuiSystem::process_event(const SDL_Event& event) {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiSystem::new_frame() {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiSystem::render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex) {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);

        SDL_GPUColorTargetInfo ct{};
        ct.texture = swap_tex;
        ct.load_op = SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &ct, 1, nullptr);
            if (!pass) {
                return;
            }

            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, pass);

            SDL_EndGPURenderPass(pass);
        }
    }

    void ImGuiSystem::discard_frame() {
        ImGui::EndFrame();
    }
} // namespace hob
