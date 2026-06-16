#pragma once

#include <filesystem>
#include <unordered_map>

#include <SDL3/SDL_gpu.h>

namespace hob {
    class Renderer;

    struct Glyph {
        // UV in [0, 1] of the atlas.
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        // Pixel dimensions of the rendered quad.
        int width = 0;
        int height = 0;
        // Offset from the pen position (top-left of the line) to the glyph's top-left in pixels.
        int offset_x = 0;
        int offset_y = 0;
        // Horizontal advance in pixels.
        int advance = 0;
    };

    // ASCII-only bitmap font wrapping SDL_ttf. Rasterizes printable glyphs (32..126)
    // into a single GPU texture at init. Non-ASCII codepoints fall back to '?'.
    class Font {
        SDL_GPUDevice* m_gpu_device = nullptr;
        SDL_GPUTexture* m_atlas = nullptr;
        uint32_t m_atlas_width = 0;
        uint32_t m_atlas_height = 0;
        int m_line_height = 0;
        std::unordered_map<uint32_t, Glyph> m_glyphs;
        bool m_initialized = false;

    public:
        Font() = default;
        ~Font();

        Font(const Font&) = delete;
        Font& operator=(const Font&) = delete;
        Font(Font&&) = delete;
        Font& operator=(Font&&) = delete;

        bool init(Renderer& renderer, const std::filesystem::path& ttf_path, float size_px);
        void shutdown();

        bool is_initialized() const;

        SDL_GPUTexture* get_atlas_texture() const;
        uint32_t get_atlas_width() const;
        uint32_t get_atlas_height() const;
        int get_line_height() const;

        // Returns the metrics for `codepoint`, or for '?' if unknown, or nullptr if
        // even '?' is missing (font is empty).
        const Glyph* get_glyph(uint32_t codepoint) const;
    };
} // namespace hob
