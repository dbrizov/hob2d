#pragma once

#include <unordered_map>

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Vertex.h>
#include <SDL3/SDL_gpu.h>

#include "engine/core/systems/renderer/texture.h"
#include "engine/math/matrix4x4.h"
#include "engine/math/vector2.h"

namespace hob {
    class SdlContext;
    class Renderer;

    class UiRenderInterface : public Rml::RenderInterface {
        static constexpr Rml::TextureHandle INVALID_TEXTURE_HANDLE = 0;

        const SdlContext& m_sdl_context;
        Renderer& m_renderer;

        SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
        SDL_GPUSampler* m_sampler = nullptr;
        TextureRef m_white_texture;
        Matrix4x4 m_projection;
        Matrix4x4 m_transform = Matrix4x4::identity();
        Vector2 m_logical_size;

        SDL_GPUCommandBuffer* m_active_cmd = nullptr;
        SDL_GPURenderPass* m_active_pass = nullptr;
        int m_target_width = 0;
        int m_target_height = 0;

        bool m_scissor_enabled = false;
        SDL_Rect m_scissor_rect{};

        std::unordered_map<Rml::TextureHandle, TextureRef> m_textures;
        Rml::TextureHandle m_next_texture_handle = INVALID_TEXTURE_HANDLE + 1;

    public:
        UiRenderInterface(const SdlContext& sdl_context, Renderer& renderer);
        ~UiRenderInterface() override;

        void init();

        Vector2 get_logical_size() const;
        void set_logical_size(const Vector2& size);

        void begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swap_tex);
        void end_frame();

        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                    Rml::Span<const int> indices) override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                            Rml::Vector2f translation,
                            Rml::TextureHandle texture) override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

        Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
        void ReleaseTexture(Rml::TextureHandle texture) override;

        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;

        void SetTransform(const Rml::Matrix4f* transform) override;
    };
} // namespace hob
