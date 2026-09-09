#include "editor_icons.h"

#include <filesystem>
#include <iterator>
#include <memory>
#include <vector>

#include <lunasvg.h>

#include "editor/editor_style.h"
#include "engine/core/assert.h"
#include "engine/core/path_utils.h"
#include "engine/core/systems/renderer/renderer.h"

namespace hob::editor {
    namespace {
        constexpr uint32_t ICON_COUNT = static_cast<uint32_t>(EditorBarIcon::Count);
        constexpr uint32_t ICON_CELL_STRIDE = ICON_SIZE_PX + ICON_PADDING_PX;
        constexpr uint32_t ICON_ATLAS_WIDTH = ICON_CELL_STRIDE * ICON_COUNT;

        constexpr const char* ICON_FILES[] = {
            "icons/play.svg",
            "icons/pause.svg",
            "icons/step.svg",
            "icons/stop.svg",
            "icons/mode_translate_rotate.svg",
            "icons/mode_translate.svg",
            "icons/mode_rotate.svg",
            "icons/mode_scale.svg",
            "icons/space_world.svg",
            "icons/space_local.svg",
        };

        static_assert(std::size(ICON_FILES) == ICON_COUNT);
    } // namespace

    void EditorIcons::load(Renderer& renderer) {
        std::vector<uint8_t> pixels(static_cast<size_t>(ICON_ATLAS_WIDTH) * ICON_SIZE_PX * 4, 0);

        for (uint32_t index = 0; index < ICON_COUNT; ++index) {
            const std::filesystem::path path = PathUtils::resolve_asset_path(ICON_FILES[index]);

            std::unique_ptr<lunasvg::Document> document = lunasvg::Document::loadFromFile(path.string());
            HOB_CHECK(document, "Failed to load the editor icon '{}'", ICON_FILES[index]);

            lunasvg::Bitmap bitmap =
                document->renderToBitmap(static_cast<int32_t>(ICON_SIZE_PX), static_cast<int32_t>(ICON_SIZE_PX));
            HOB_CHECK(!bitmap.isNull(), "Failed to rasterise the editor icon '{}'", ICON_FILES[index]);

            bitmap.convertToRGBA();

            const uint8_t* source = bitmap.data();
            const size_t source_stride = static_cast<size_t>(bitmap.stride());
            const uint32_t cell_x = index * ICON_CELL_STRIDE;

            for (uint32_t y = 0; y < ICON_SIZE_PX; ++y) {
                for (uint32_t x = 0; x < ICON_SIZE_PX; ++x) {
                    const uint8_t* texel = source + y * source_stride + static_cast<size_t>(x) * 4;
                    uint8_t* target = pixels.data() + ((static_cast<size_t>(y) * ICON_ATLAS_WIDTH) + cell_x + x) * 4;

                    // ImDrawList::AddImage multiplies the texel by the tint colour, so the icons are stored as a
                    // white alpha mask for the bar's state colours to come through unchanged.
                    target[0] = 255;
                    target[1] = 255;
                    target[2] = 255;
                    target[3] = texel[3];
                }
            }
        }

        m_atlas = renderer.create_texture_from_rgba(pixels.data(), ICON_ATLAS_WIDTH, ICON_SIZE_PX);
        HOB_CHECK(m_atlas, "Failed to create the editor icon atlas texture");
    }

    bool EditorIcons::is_loaded() const {
        return m_atlas != nullptr;
    }

    SDL_GPUTexture* EditorIcons::get_texture() const {
        return m_atlas ? m_atlas->get_gpu_texture() : nullptr;
    }

    ImVec2 EditorIcons::get_uv_min(EditorBarIcon icon) const {
        const float x = static_cast<float>(static_cast<uint32_t>(icon) * ICON_CELL_STRIDE);
        return ImVec2(x / static_cast<float>(ICON_ATLAS_WIDTH), 0.0f);
    }

    ImVec2 EditorIcons::get_uv_max(EditorBarIcon icon) const {
        const float x = static_cast<float>(static_cast<uint32_t>(icon) * ICON_CELL_STRIDE + ICON_SIZE_PX);
        return ImVec2(x / static_cast<float>(ICON_ATLAS_WIDTH), 1.0f);
    }
} // namespace hob::editor
