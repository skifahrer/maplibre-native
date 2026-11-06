#include <mln/renderer/texture_pool.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/offscreen_texture.hpp>
#include <mln/gfx/texture2d.hpp>
#include <mln/util/logging.hpp>

#include <algorithm>

namespace mln {
TexturePool::TexturePool(uint32_t tilesize)
    : tileSize(tilesize) {}

TexturePool::~TexturePool() = default;

void TexturePool::createRenderTarget(gfx::Context& context,
                                     const UnwrappedTileID& id,
                                     const Color& backgroundColor,
                                     uint32_t size) {
    const uint32_t side = size ? size : tileSize;
    // Keep an existing render target; recreating it every frame churns GL
    // texture memory and invalidates the texture bound to the terrain drawable
    if (auto it = renderTargets.find(id); it != renderTargets.end() && it->second) {
        if (it->second->getTexture()->getSize().width == side) {
            it->second->setClearColor(backgroundColor);
            return;
        }
    }
    if (auto it = renderTargets.find(id); it != renderTargets.end() && it->second) {
        retire(std::move(it->second));
    }
    auto renderTarget = reuse(context, side);
    if (!renderTarget) {
        // Drape targets render several overlapping draped tiles (a parent standing in for missing
        // children beside already-loaded children); the stencil clips them against each other,
        // as maplibre-gl-js does for its render-to-texture framebuffer.
        renderTarget = context.createRenderTarget(
            {side, side}, gfx::TextureChannelDataType::UnsignedByte, /*stencil=*/true);
    }
    renderTarget->setClearColor(backgroundColor);
    renderTarget->setDrapeTileID(id);
    renderTargets[id] = std::move(renderTarget);
}

void TexturePool::removeStaleRenderTargets(const std::set<UnwrappedTileID>& currentTiles) {
    // Freed if no new target took it within a frame, so the budget is overrun only briefly.
    std::erase_if(retired, [&](const Retired& old) { return currentTiles.empty() || old.frame < frame; });
    for (auto it = renderTargets.begin(); it != renderTargets.end();) {
        if (!currentTiles.contains(it->first)) {
            if (!currentTiles.empty() && it->second) {
                retire(std::move(it->second));
            }
            it = renderTargets.erase(it);
        } else {
            ++it;
        }
    }
    ++frame;
}

void TexturePool::retire(std::shared_ptr<RenderTarget> target) {
    // A zoom step retires the whole cover at once; past the cap they go as before.
    const auto& texture = target->getTexture();
    const std::size_t texels = texture ? std::size_t{texture->getSize().width} * texture->getSize().height : 0;
    if (retiredTexels() + texels <= maxRetiredTexels) {
        retired.push_back({std::move(target), frame});
    }
}

std::size_t TexturePool::retiredTexels() const {
    std::size_t texels = 0;
    for (const auto& old : retired) {
        if (const auto& texture = old.target->getTexture()) {
            texels += std::size_t{texture->getSize().width} * texture->getSize().height;
        }
    }
    return texels;
}

std::shared_ptr<RenderTarget> TexturePool::reuse(gfx::Context& context, uint32_t side) {
    // Only once the orchestrator has let go of a target is its texture free to draw into.
    const auto free = std::ranges::find_if(retired, [&](const Retired& old) {
        return old.target.use_count() == 1 && old.target->getTexture()->getSize().width == side;
    });
    if (free == retired.end()) {
        return nullptr;
    }
    auto texture = free->target->takeOffscreenTexture();
    retired.erase(free);
    return std::make_shared<RenderTarget>(context, std::move(texture));
}

std::shared_ptr<RenderTarget> TexturePool::getRenderTarget(const UnwrappedTileID& id) const {
    return renderTargets.contains(id) ? renderTargets.at(id) : nullptr;
}

std::shared_ptr<RenderTarget> TexturePool::getRenderTargetAncestorOrDescendant(
    const UnwrappedTileID& id, std::optional<UnwrappedTileID>& terrainTileID) const {
    terrainTileID = std::nullopt;
    std::shared_ptr<RenderTarget> bestRenderTarget;
    for (const auto& [tileID, renderTarget] : renderTargets) {
        if (tileID == id || tileID.isChildOf(id) || id.isChildOf(tileID)) {
            if (!terrainTileID || tileID.canonical.z > terrainTileID->canonical.z) {
                terrainTileID = tileID;
                bestRenderTarget = renderTarget;
            }
        }
    }
    return bestRenderTarget;
}
} // namespace mln
