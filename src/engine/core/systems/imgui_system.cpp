#include "imgui_system.h"

#include <filesystem>
#include <string>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include "engine/core/assert.h"
#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/window.h"
#include "renderer/renderer.h"

namespace hob {
    ImGuiSystem::ImGuiSystem(const Renderer& renderer)
        : m_renderer(renderer) {
        SDL_Window* window = m_renderer.get_main_window()->get_window();
        HOB_CHECK(window != nullptr && renderer.get_gpu_device() != nullptr,
                  "ImGuiSystem init failed: window/GPU device is null");

        IMGUI_CHECKVERSION();

        m_context = ImGui::CreateContext();
        HOB_CHECK(m_context != nullptr, "ImGui::CreateContext failed");

        log::imgui.info("ImGui_CreateContext()");

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const std::filesystem::path font_path =
            PathUtils::get_engine_assets_root() / "fonts" / "jetbrains_mono_bold.ttf";
        const std::string font_path_str = font_path.string();
        ImFont* font = io.Fonts->AddFontFromFileTTF(font_path_str.c_str());
        HOB_CHECK(font != nullptr, "Failed to load ImGui font: {}", font_path_str);

        ImGui::StyleColorsDark();

        const bool sdl3_initialized = ImGui_ImplSDL3_InitForSDLGPU(window);
        HOB_CHECK(sdl3_initialized, "ImGui_ImplSDL3_InitForSDLGPU failed");

        log::imgui.info("ImGui_ImplSDL3_InitForSDLGPU");

        ImGui_ImplSDLGPU3_InitInfo init_info{};
        init_info.Device = m_renderer.get_gpu_device();
        init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(m_renderer.get_gpu_device(), window);
        init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;

        const bool gpu_initialized = ImGui_ImplSDLGPU3_Init(&init_info);
        HOB_CHECK(gpu_initialized, "ImGui_ImplSDLGPU3_Init failed");

        log::imgui.info("ImGui_ImplSDLGPU3_Init");
    }

    ImGuiSystem::~ImGuiSystem() {
        ImGui_ImplSDLGPU3_Shutdown();
        log::imgui.info("ImGui_ImplSDLGPU3_Shutdown");

        ImGui_ImplSDL3_Shutdown();
        log::imgui.info("ImGui_ImplSDL3_Shutdown");

        ImGui::DestroyContext(m_context);
        m_context = nullptr;
        log::imgui.info("ImGui_DestroyContext");
    }

    void ImGuiSystem::set_clear_color(const ImVec4& color) {
        m_clear_color = SDL_FColor{color.x, color.y, color.z, color.w};
    }

    void ImGuiSystem::set_clear_swapchain(bool clear) {
        m_clear_swapchain = clear;
    }

    void ImGuiSystem::process_event(const SDL_Event& event) {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    void ImGuiSystem::new_frame() {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiSystem::render_pass() {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, m_renderer.get_command_buffer());

        SDL_GPUColorTargetInfo ct{};
        ct.texture = m_renderer.get_main_swap_texture();
        ct.clear_color = m_clear_color;
        ct.load_op = m_clear_swapchain ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        ct.store_op = SDL_GPU_STOREOP_STORE;

        // Render pass
        {
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(m_renderer.get_command_buffer(), &ct, 1, nullptr);
            if (pass == nullptr) {
                return;
            }

            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, m_renderer.get_command_buffer(), pass);

            SDL_EndGPURenderPass(pass);
        }
    }

    void ImGuiSystem::discard_frame() {
        ImGui::EndFrame();
    }
} // namespace hob
