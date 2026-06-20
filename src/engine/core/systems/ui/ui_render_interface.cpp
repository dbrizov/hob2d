#include "ui_render_interface.h"

namespace hob {
    UiRenderInterface::UiRenderInterface(const SdlContext& sdl_context, Renderer& renderer)
        : m_sdl_context(sdl_context)
        , m_renderer(renderer) {}

    Rml::CompiledGeometryHandle UiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                   Rml::Span<const int> indices) {
        return 0;
    }

    void UiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                           Rml::Vector2f translation,
                                           Rml::TextureHandle texture) {}

    void UiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {}

    Rml::TextureHandle UiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions,
                                                      const Rml::String& source) {
        return 0;
    }

    Rml::TextureHandle UiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                          Rml::Vector2i source_dimensions) {
        return 0;
    }

    void UiRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {}

    void UiRenderInterface::EnableScissorRegion(bool enable) {}

    void UiRenderInterface::SetScissorRegion(Rml::Rectanglei region) {}
} // namespace hob
