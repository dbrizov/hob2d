#include <cstring>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "engine/core/logging.h"
#include "engine/core/path_utils.h"
#include "engine/math/constants.h"
#include "mesh_gltf.h"
#include "mesh_primitives.h"
#include "renderer.h"

namespace hob {
    TextureRef Renderer::get_cached_texture(std::string_view key) const {
        const auto it = m_textures.find(key);
        if (it == m_textures.end()) {
            return TextureRef();
        }

        TextureRef cached = it->second.lock();
        if (cached && m_cvar_log_textures) {
            log::renderer.info("Renderer::get_or_load_texture cache hit: '{}' (rc={})", key, cached.use_count());
        }

        return cached;
    }

    TextureRef Renderer::get_or_load_texture(std::string_view relative_path) {
        if (TextureRef cached = get_cached_texture(relative_path)) {
            return cached;
        }

        const std::string key = std::filesystem::path(relative_path).lexically_normal().generic_string();
        if (key != relative_path) {
            if (TextureRef cached = get_cached_texture(key)) {
                return cached;
            }
        }

        const std::filesystem::path full_path = PathUtils::resolve_asset_path(relative_path);
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
        m_default_mesh_shader.reset();
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
        m_default_mesh_material.reset();
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

        constexpr bool cycle = false;
        void* map = SDL_MapGPUTransferBuffer(m_gpu_device, tb, cycle);
        if (!map) {
            log::renderer.error("SDL_MapGPUTransferBuffer (texture) failed: {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
            return false;
        }
        std::memcpy(map, pixels, size);
        SDL_UnmapGPUTransferBuffer(m_gpu_device, tb);

        SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(m_gpu_device);
        if (!upload_cmd) {
            log::renderer.error("SDL_AcquireGPUCommandBuffer (texture) failed: {}", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
            return false;
        }

        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);
        if (!copy) {
            log::renderer.error("SDL_BeginGPUCopyPass (texture) failed: {}", SDL_GetError());
            SDL_CancelGPUCommandBuffer(upload_cmd);
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
            return false;
        }

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

        SDL_UploadToGPUTexture(copy, &src, &dst, cycle);

        SDL_EndGPUCopyPass(copy);

        SDL_SubmitGPUCommandBuffer(upload_cmd);
        SDL_ReleaseGPUTransferBuffer(m_gpu_device, tb);
        return true;
    }

    bool Renderer::upload_buffer(SDL_GPUBuffer* dst_buffer, const void* data, uint32_t size) {
        if (size == 0) {
            return true;
        }

        if (!ensure_upload_transfer_buffer(size)) {
            return false;
        }

        constexpr bool cycle = true;
        void* map = SDL_MapGPUTransferBuffer(m_gpu_device, m_upload_transfer_buffer, cycle);
        if (!map) {
            log::renderer.error("SDL_MapGPUTransferBuffer failed: {}", SDL_GetError());
            return false;
        }
        std::memcpy(map, data, size);
        SDL_UnmapGPUTransferBuffer(m_gpu_device, m_upload_transfer_buffer);

        SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(m_gpu_device);
        if (!upload_cmd) {
            log::renderer.error("SDL_AcquireGPUCommandBuffer (buffer) failed: {}", SDL_GetError());
            return false;
        }

        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(upload_cmd);
        if (!copy) {
            log::renderer.error("SDL_BeginGPUCopyPass (buffer) failed: {}", SDL_GetError());
            SDL_CancelGPUCommandBuffer(upload_cmd);
            return false;
        }

        SDL_GPUTransferBufferLocation src{};
        src.transfer_buffer = m_upload_transfer_buffer;
        src.offset = 0;
        SDL_GPUBufferRegion dst{};
        dst.buffer = dst_buffer;
        dst.offset = 0;
        dst.size = size;
        SDL_UploadToGPUBuffer(copy, &src, &dst, cycle);

        SDL_EndGPUCopyPass(copy);

        SDL_SubmitGPUCommandBuffer(upload_cmd);
        return true;
    }

    bool Renderer::ensure_upload_transfer_buffer(uint32_t size) {
        if (m_upload_transfer_buffer != nullptr && m_upload_transfer_capacity >= size) {
            return true;
        }

        uint32_t capacity = m_upload_transfer_capacity > 0 ? m_upload_transfer_capacity : 64 * 1024;
        while (capacity < size) {
            if (capacity > MAX_UINT32 / 2) {
                capacity = size;
                break;
            }

            capacity *= 2;
        }

        SDL_GPUTransferBufferCreateInfo tbi{};
        tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tbi.size = capacity;
        SDL_GPUTransferBuffer* grown = SDL_CreateGPUTransferBuffer(m_gpu_device, &tbi);
        if (!grown) {
            log::renderer.error("SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError());
            return false;
        }

        if (m_upload_transfer_buffer != nullptr) {
            SDL_ReleaseGPUTransferBuffer(m_gpu_device, m_upload_transfer_buffer);
        }

        m_upload_transfer_buffer = grown;
        m_upload_transfer_capacity = capacity;
        return true;
    }
    ShaderRef Renderer::get_default_mesh_shader() const {
        return m_default_mesh_shader;
    }

    MaterialRef Renderer::get_default_mesh_material() const {
        return m_default_mesh_material;
    }

    MeshRef Renderer::create_mesh(const MeshData& data, std::string source) {
        if (data.vertices.empty() || data.indices.empty()) {
            log::renderer.error("Renderer::create_mesh '{}': empty mesh data", source);
            return nullptr;
        }

        const uint32_t vertex_bytes = static_cast<uint32_t>(data.vertices.size() * sizeof(MeshVertex));
        const uint32_t index_bytes = static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t));

        SDL_GPUBufferCreateInfo vbi{};
        vbi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vbi.size = vertex_bytes;
        SDL_GPUBuffer* vbo = SDL_CreateGPUBuffer(m_gpu_device, &vbi);
        if (!vbo) {
            log::renderer.error("SDL_CreateGPUBuffer (mesh vertices '{}') failed: {}", source, SDL_GetError());
            return nullptr;
        }

        SDL_GPUBufferCreateInfo ibi{};
        ibi.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ibi.size = index_bytes;
        SDL_GPUBuffer* ibo = SDL_CreateGPUBuffer(m_gpu_device, &ibi);
        if (!ibo) {
            log::renderer.error("SDL_CreateGPUBuffer (mesh indices '{}') failed: {}", source, SDL_GetError());
            SDL_ReleaseGPUBuffer(m_gpu_device, vbo);
            return nullptr;
        }

        if (!upload_buffer(vbo, data.vertices.data(), vertex_bytes) ||
            !upload_buffer(ibo, data.indices.data(), index_bytes)) {
            SDL_ReleaseGPUBuffer(m_gpu_device, ibo);
            SDL_ReleaseGPUBuffer(m_gpu_device, vbo);
            return nullptr;
        }

        MeshRef mesh(new Mesh(m_gpu_device,
                              vbo,
                              ibo,
                              static_cast<uint32_t>(data.vertices.size()),
                              static_cast<uint32_t>(data.indices.size()),
                              data.bounds,
                              source));
        m_meshes[source] = mesh;
        return mesh;
    }

    MeshRef Renderer::get_or_create_primitive_mesh(std::string_view name) {
        const std::string key = "primitive:" + std::string(name);
        const auto it = m_meshes.find(key);
        if (it != m_meshes.end()) {
            return it->second;
        }

        const std::optional<MeshData> data = mesh_primitives::make_by_name(name);
        if (!data.has_value()) {
            log::renderer.error("Renderer: unknown primitive mesh '{}' (expected cube|plane|sphere|capsule)", name);
            return nullptr;
        }

        return create_mesh(*data, key);
    }

    MeshRef Renderer::get_or_load_mesh(std::string_view relative_path) {
        const std::string key = std::filesystem::path(relative_path).lexically_normal().generic_string();
        const auto it = m_meshes.find(key);
        if (it != m_meshes.end()) {
            return it->second;
        }

        std::string error;
        const std::optional<MeshData> data = mesh_gltf::load(PathUtils::resolve_asset_path(relative_path), error);
        if (!data.has_value()) {
            log::renderer.error("Renderer: failed to load mesh '{}': {}", key, error);
            return nullptr;
        }

        log::renderer.info("Renderer: loaded mesh '{}' ({} vertices, {} triangles)",
                           key,
                           data->vertices.size(),
                           data->indices.size() / 3);
        return create_mesh(*data, key);
    }

    void Renderer::release_meshes() {
        m_meshes.clear();
    }
} // namespace hob
