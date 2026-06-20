#include "font.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "engine/core/logging.h"
#include "renderer.h"

namespace hob {
    namespace {
        constexpr uint32_t FIRST_GLYPH = 32;
        constexpr uint32_t LAST_GLYPH = 126;
        constexpr int GLYPH_PADDING = 1;

        struct BakedGlyph {
            uint32_t codepoint;
            SDL_Surface* surface;
            int min_x;
            int max_y;
            int advance;
        };
    } // namespace

    Font::~Font() {
        shutdown();
    }

    bool Font::init(Renderer& renderer, const std::filesystem::path& ttf_path, float size_px) {
        if (m_initialized) {
            return true;
        }

        if (!TTF_Init()) {
            log::renderer.error("TTF_Init failed: {}", SDL_GetError());
            return false;
        }

        TTF_Font* font = TTF_OpenFont(ttf_path.string().c_str(), size_px);
        if (!font) {
            log::renderer.error("TTF_OpenFont('{}') failed: {}", ttf_path.string(), SDL_GetError());
            TTF_Quit();
            return false;
        }

        m_line_height = TTF_GetFontHeight(font);

        // Rasterize all glyphs first; we need their sizes before sizing the atlas.
        std::vector<BakedGlyph> baked;
        baked.reserve(LAST_GLYPH - FIRST_GLYPH + 1);
        int row_h = 0;
        int total_w = 0;

        const SDL_Color white{255, 255, 255, 255};
        for (uint32_t cp = FIRST_GLYPH; cp <= LAST_GLYPH; ++cp) {
            SDL_Surface* surf = TTF_RenderGlyph_Blended(font, cp, white);
            if (!surf) {
                continue;
            }

            int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
            TTF_GetGlyphMetrics(font, cp, &min_x, &max_x, &min_y, &max_y, &advance);

            total_w += surf->w + GLYPH_PADDING;
            row_h = std::max(row_h, surf->h);
            baked.push_back({cp, surf, min_x, max_y, advance});
        }

        if (baked.empty() || row_h == 0 || total_w == 0) {
            log::renderer.error("Font '{}' produced no glyphs", ttf_path.string());
            TTF_CloseFont(font);
            TTF_Quit();
            return false;
        }

        m_atlas_width = static_cast<uint32_t>(total_w);
        m_atlas_height = static_cast<uint32_t>(row_h);

        std::vector<uint8_t> atlas_pixels(static_cast<size_t>(m_atlas_width) * m_atlas_height * 4, 0);

        int pen_x = 0;
        for (const BakedGlyph& b : baked) {
            SDL_Surface* surf = b.surface;

            // TTF_RenderGlyph_Blended returns ARGB8888 / RGBA32 depending on platform;
            // normalize to RGBA32 so the atlas upload is uniform.
            SDL_Surface* converted = surf;
            bool free_converted = false;
            if (surf->format != SDL_PIXELFORMAT_RGBA32) {
                converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
                free_converted = true;
            }

            if (converted) {
                const uint8_t* src = static_cast<const uint8_t*>(converted->pixels);
                for (int y = 0; y < converted->h; ++y) {
                    uint8_t* dst_row = atlas_pixels.data() + (static_cast<size_t>(y) * m_atlas_width + pen_x) * 4;
                    const uint8_t* src_row = src + static_cast<size_t>(y) * converted->pitch;
                    std::memcpy(dst_row, src_row, static_cast<size_t>(converted->w) * 4);
                }
            }

            Glyph g{};
            g.u0 = static_cast<float>(pen_x) / static_cast<float>(m_atlas_width);
            g.v0 = 0.0f;
            g.u1 = static_cast<float>(pen_x + surf->w) / static_cast<float>(m_atlas_width);
            g.v1 = static_cast<float>(surf->h) / static_cast<float>(m_atlas_height);
            g.width = surf->w;
            g.height = surf->h;
            g.offset_x = b.min_x;
            // SDL_ttf places each glyph at y = (ascent - sz_top) inside the rendered
            // surface, so the per-glyph vertical position relative to the line top is
            // already baked into the surface itself. No additional offset needed.
            g.offset_y = 0;
            g.advance = b.advance;
            m_glyphs.emplace(b.codepoint, g);

            pen_x += surf->w + GLYPH_PADDING;
            if (free_converted) {
                SDL_DestroySurface(converted);
            }
            SDL_DestroySurface(surf);
        }

        TTF_CloseFont(font);

        m_gpu_device = renderer.m_gpu_device;

        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = m_atlas_width;
        tci.height = m_atlas_height;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        m_atlas = SDL_CreateGPUTexture(m_gpu_device, &tci);
        if (!m_atlas) {
            log::renderer.error("SDL_CreateGPUTexture (font atlas) failed: {}", SDL_GetError());
            m_gpu_device = nullptr;
            m_glyphs.clear();
            TTF_Quit();
            return false;
        }

        if (!renderer.upload_texture_rgba(m_atlas, atlas_pixels.data(), m_atlas_width, m_atlas_height)) {
            log::renderer.error("Font atlas upload failed for '{}'", ttf_path.string());
            SDL_ReleaseGPUTexture(m_gpu_device, m_atlas);
            m_atlas = nullptr;
            m_gpu_device = nullptr;
            m_glyphs.clear();
            TTF_Quit();
            return false;
        }

        m_initialized = true;
        return true;
    }

    void Font::shutdown() {
        if (!m_initialized && !m_atlas) {
            return;
        }

        if (m_atlas && m_gpu_device) {
            SDL_ReleaseGPUTexture(m_gpu_device, m_atlas);
        }
        m_atlas = nullptr;
        m_gpu_device = nullptr;
        m_glyphs.clear();
        m_atlas_width = 0;
        m_atlas_height = 0;
        m_line_height = 0;

        if (m_initialized) {
            TTF_Quit();
            m_initialized = false;
        }
    }

    bool Font::is_initialized() const {
        return m_initialized;
    }

    SDL_GPUTexture* Font::get_atlas_texture() const {
        return m_atlas;
    }

    uint32_t Font::get_atlas_width() const {
        return m_atlas_width;
    }

    uint32_t Font::get_atlas_height() const {
        return m_atlas_height;
    }

    int Font::get_line_height() const {
        return m_line_height;
    }

    const Glyph* Font::get_glyph(uint32_t codepoint) const {
        auto it = m_glyphs.find(codepoint);
        if (it != m_glyphs.end()) {
            return &it->second;
        }
        // Fallback to '?' for unsupported codepoints.
        it = m_glyphs.find('?');
        if (it != m_glyphs.end()) {
            return &it->second;
        }
        return nullptr;
    }
} // namespace hob
