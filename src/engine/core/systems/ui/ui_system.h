#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

#include "engine/math/vector2.h"
#include "ui_file_interface.h"
#include "ui_render_interface.h"
#include "ui_screen_match_mode.h"
#include "ui_system_interface.h"

namespace Rml {
    class Context;
    class ElementDocument;
    class StyleSheetContainer;
} // namespace Rml

namespace hob {
    struct UiSystemConfig;
    class SdlContext;
    class Renderer;
    class Timer;

    using UiDocumentId = int64_t;
    constexpr UiDocumentId INVALID_UI_DOCUMENT_ID = -1;

    struct UiDocument {
        Rml::ElementDocument* rml_document = nullptr;
        std::string path;
    };

    class UiSystem {
        const SdlContext& m_sdl_context;
        const Renderer& m_renderer;

        bool m_is_initialized = false;

        UiFileInterface m_file_interface;
        UiSystemInterface m_system_interface;
        UiRenderInterface m_render_interface;

        Rml::Context* m_context = nullptr;
        std::shared_ptr<Rml::StyleSheetContainer> m_base_stylesheet;

        std::unordered_map<UiDocumentId, UiDocument> m_documents;
        UiDocumentId m_next_document_id = 0;

        Vector2 m_reference_resolution;
        UiScreenMatchMode m_screen_match_mode;
        int m_last_window_width = 0;
        int m_last_window_height = 0;

    public:
        UiSystem(const UiSystemConfig& config, const SdlContext& sdl_context, Renderer& renderer, const Timer& timer);
        ~UiSystem();

        UiSystem(const UiSystem&) = delete;
        UiSystem& operator=(const UiSystem&) = delete;

        UiSystem(UiSystem&&) = delete;
        UiSystem& operator=(UiSystem&&) = delete;

        bool is_initialized() const;

        void process_event(const SDL_Event& event);

        void tick();

        void render_pass(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex);

        UiDocumentId load_document(const std::string& path);
        void show_document(UiDocumentId id);
        void hide_document(UiDocumentId id);

    private:
        UiDocument* find_document(UiDocumentId id);
        void apply_base_stylesheet(Rml::ElementDocument& document) const;
        Vector2 compute_effective_logical_size(int window_width, int window_height) const;
        void update_logical_size();
    };
} // namespace hob
