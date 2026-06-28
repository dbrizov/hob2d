#include <cstring>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "renderer.h"

namespace hob {
    TextureRef Renderer::get_or_load_texture(const std::string& path) {
        const std::string key = std::filesystem::path(path).lexically_normal().string();

        auto tex_it = m_textures.find(key);
        if (tex_it != m_textures.end()) {
            if (auto cached = tex_it->second.lock()) {
                if (m_cvar_log_textures) {
                    log::renderer.info(
                        "Renderer::get_or_load_texture cache hit: '{}' (rc={})", key, cached.use_count());
                }
                return cached;
            }
        }

        const std::filesystem::path full_path = PathUtils::get_assets_root_path() / path;
        SDL_Surface* surface = IMG_Load(full_path.string().c_str());
        if (!surface) {
            log::renderer.error("IMG_Load failed: {}", SDL_GetError());
            return TextureRef();
        }

        SDL_Surface* rgba = surface;
        if (surface->format != SDL_PIXELFORMAT_RGBA32) {
            rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!rgba) {
                log::renderer.error("SDL_ConvertSurface failed: {}", SDL_GetError());
                return TextureRef();
            }
        }

        const uint32_t w = static_cast<uint32_t>(rgba->w);
        const uint32_t h = static_cast<uint32_t>(rgba->h);

        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = w;
        tci.height = h;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* gpu_tex = SDL_CreateGPUTexture(m_gpu_device, &tci);
        if (!gpu_tex) {
            log::renderer.error("SDL_CreateGPUTexture failed: {}", SDL_GetError());
            SDL_DestroySurface(rgba);
            return TextureRef();
        }

        if (!upload_texture_rgba(gpu_tex, rgba->pixels, w, h)) {
            SDL_ReleaseGPUTexture(m_gpu_device, gpu_tex);
            SDL_DestroySurface(rgba);
            return TextureRef();
        }

        SDL_DestroySurface(rgba);

        TextureRef texture(new Texture(*this, gpu_tex, w, h, key));
        m_textures.emplace(key, texture);

        if (m_cvar_log_textures) {
            log::renderer.info("Renderer::get_or_load_texture loaded: '{}' (rc=1)", key);
        }

        return texture;
    }

    TextureRef Renderer::create_texture_from_rgba(const void* pixels, uint32_t width, uint32_t height) {
        SDL_GPUTextureCreateInfo tci{};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = width;
        tci.height = height;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* gpu_tex = SDL_CreateGPUTexture(m_gpu_device, &tci);
        if (!gpu_tex) {
            log::renderer.error("SDL_CreateGPUTexture (from rgba) failed: {}", SDL_GetError());
            return TextureRef();
        }

        if (!upload_texture_rgba(gpu_tex, pixels, width, height)) {
            SDL_ReleaseGPUTexture(m_gpu_device, gpu_tex);
            return TextureRef();
        }

        return TextureRef(new Texture(*this, gpu_tex, width, height, ""));
    }

    void Renderer::release_texture(Texture& texture) {
        if (m_cvar_log_textures) {
            log::renderer.info("Renderer::release_texture: '{}' [destroyed]", texture.m_path);
        }

        m_textures.erase(texture.m_path);
        if (texture.m_gpu_texture) {
            SDL_ReleaseGPUTexture(m_gpu_device, texture.m_gpu_texture);
            texture.m_gpu_texture = nullptr;
        }

        texture.m_renderer = nullptr;
    }

    void Renderer::release_textures() {
        // Defensive sweep: with the engine's subsystem destruction order, every TextureRef
        // holder dies before the Renderer, so the weak refs here should already be expired.
        // If anything is still alive, detach it from the renderer and release its GPU handle
        // directly so Texture::~Texture (which would call back into us) becomes a no-op.
        m_fallback_texture.reset();
        std::vector<TextureRef> alive;
        for (auto& [path, weak] : m_textures) {
            if (auto texture = weak.lock()) {
                log::renderer.error(
                    "Renderer::~Renderer: texture '{}' still has {} holder(s) at shutdown — destruction order is wrong",
                    path,
                    texture.use_count() - 1);

                alive.push_back(std::move(texture));
            }
        }

        for (auto& texture : alive) {
            release_texture(*texture);
        }
    }

    void Renderer::release_shaders() {
        // The map holds one strong ref per shader; anything beyond that is an external holder (e.g. a
        // surviving Material) that outlived the Renderer and will free its pipeline against a possibly
        // dead GPU device. Diagnose before clearing the map.
        m_default_shader.reset();
        for (const auto& [key, shader] : m_shaders) {
            if (shader.use_count() > 1) {
                log::renderer.error(
                    "Renderer::~Renderer: shader '{}' still has {} external holder(s) at shutdown — destruction order is wrong",
                    shader->get_path(),
                    shader.use_count() - 1);
            }
        }
        m_shaders.clear();
    }

    void Renderer::release_materials() {
        // Diagnostic only: a Material owns no GPU handle and has no renderer callback, so a survivor
        // is harmless in itself. But by the destruction order every MaterialRef holder should be gone
        // before the Renderer, so a live one here flags a leak / wrong destruction order.
        // Call .lock() adds a temporary +1, so subtract it to report the real external holder count.
        m_default_material.reset();
        for (const auto& weak : m_materials) {
            if (auto material = weak.lock()) {
                log::renderer.error(
                    "Renderer::~Renderer: material '{}' still has {} holder(s) at shutdown — destruction order is wrong",
                    material->get_name().empty() ? "<inline>" : material->get_name().c_str(),
                    material.use_count() - 1);
            }
        }
        m_materials.clear();
    }

    void Renderer::track_material(const MaterialRef& material) {
        std::erase_if(m_materials, [](const MaterialWeakRef& weak) {
            return weak.expired();
        });
        m_materials.push_back(material);
    }

    bool Renderer::upload_buffer(SDL_GPUBuffer* dst_buffer, const void* data, uint32_t size) {
        SDL_GPUTransferBufferCreateInfo tbi{};
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = size;
        SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(m_gpu_device, &tbi);
        if (!tb) {
            log::renderer.error("SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError());
            return false;
        }

        void* map = SDL_MapGPUTransferBuffer(m_gpu_device, tb, false);
        if (!map) {
            log::renderer.error("SDL_MapGPUTransferBuffer failed: {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
            return false;
        }
        std::memcpy(map, data, size);
        SDL_UnmapGPUTransferBuffer(m_gpu_device, tb);

        SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(m_gpu_device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);

        SDL_GPUTransferBufferLocation src{};
        src.transfer_buffer = tb;
        src.offset = 0;
        SDL_GPUBufferRegion dst{};
        dst.buffer = dst_buffer;
        dst.offset = 0;
        dst.size = size;
        SDL_UploadToGPUBuffer(copy, &src, &dst, false);

        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(upload_cmd);
        if (fence) {
            SDL_WaitForGPUFences(m_gpu_device, true, &fence, 1);
            SDL_ReleaseGPUFence(m_gpu_device, fence);
        }
        SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
        return true;
    }

    bool Renderer::upload_texture_rgba(SDL_GPUTexture* dst_texture,
                                       const void* pixels,
                                       uint32_t width,
                                       uint32_t height) {
        const uint32_t size = width * height * 4;

        SDL_GPUTransferBufferCreateInfo tbi{};
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = size;
        SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(m_gpu_device, &tbi);
        if (!tb) {
            log::renderer.error("SDL_CreateGPUTransferBuffer (texture) failed: {}", SDL_GetError());
            return false;
        }

        void* map = SDL_MapGPUTransferBuffer(m_gpu_device, tb, false);
        if (!map) {
            log::renderer.error("SDL_MapGPUTransferBuffer (texture) failed: {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
            return false;
        }
        std::memcpy(map, pixels, size);
        SDL_UnmapGPUTransferBuffer(m_gpu_device, tb);

        SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(m_gpu_device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);

        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = tb;
        src.offset = 0;
        src.pixels_per_row = width;
        src.rows_per_layer = height;

        SDL_GPUTextureRegion dst{};
        dst.texture = dst_texture;
        dst.mip_level = 0;
        dst.layer = 0;
        dst.x = 0;
        dst.y = 0;
        dst.z = 0;
        dst.w = width;
        dst.h = height;
        dst.d = 1;

        SDL_UploadToGPUTexture(copy, &src, &dst, false);

        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(upload_cmd);
        if (fence) {
            SDL_WaitForGPUFences(m_gpu_device, true, &fence, 1);
            SDL_ReleaseGPUFence(m_gpu_device, fence);
        }
        SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
        return true;
    }
} // namespace hob
