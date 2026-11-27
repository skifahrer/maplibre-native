#include <mln/renderer/texture_pool.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/texture2d.hpp>
#include <mln/util/logging.hpp>

#include <algorithm>

namespace mln {
namespace {
std::size_t texelsOf(RenderTarget& target) {
    const auto size = target.getTexture()->getSize();
    return std::size_t{size.width} * size.height;
}
} // namespace

TexturePool::TexturePool(uint32_t tilesize)
    : tileSize(tilesize) {}

TexturePool::~TexturePool() = default;

void TexturePool::createRenderTarget(gfx::Context& context,
                                     const UnwrappedTileID& id,
                                     const Color& backgroundColor,
                                     uint32_t size) {
    const uint32_t side = size ? size : tileSize;
    // A tile coming back takes the target it left, drawn, so it shows before it re-renders
    if (auto back = kept.find(id); back != kept.end()) {
        auto target = std::move(back->second.target);
        keptTexelsHeld -= texelsOf(*target);
        kept.erase(back);
        if (target->getTexture()->getSize().width == side) {
            target->setClearColor(backgroundColor);
            renderTargets[id] = std::move(target);
            return;
        }
        retired.add(std::move(target));
        context.renderingStats().numDrapeTargetsFreed++;
    }
    // Keep an existing render target; recreating it every frame churns GL
    // texture memory and invalidates the texture bound to the terrain drawable
    if (auto it = renderTargets.find(id); it != renderTargets.end() && it->second) {
        if (it->second->getTexture()->getSize().width == side) {
            it->second->setClearColor(backgroundColor);
            return;
        }
        retired.add(std::move(it->second));
        context.renderingStats().numDrapeTargetsFreed++;
    }
    // Drape targets render several overlapping draped tiles (a parent standing in for missing
    // children beside already-loaded children); the stencil clips them against each other,
    // as maplibre-gl-js does for its render-to-texture framebuffer.
    auto renderTarget = context.createRenderTarget(
        {side, side}, gfx::TextureChannelDataType::UnsignedByte, /*stencil=*/true);
    renderTarget->setClearColor(backgroundColor);
    renderTarget->setDrapeTileID(id);
    renderTargets[id] = std::move(renderTarget);
    context.renderingStats().numDrapeTargetsMade++;
}

std::size_t TexturePool::removeStaleRenderTargets(const std::set<UnwrappedTileID>& currentTiles,
                                                  const std::size_t keptTexels) {
    ++departures;
    for (auto it = renderTargets.begin(); it != renderTargets.end();) {
        if (!currentTiles.contains(it->first)) {
            if (it->second) {
                keptTexelsHeld += texelsOf(*it->second);
                kept[it->first] = {.target = std::move(it->second), .left = departures};
            }
            it = renderTargets.erase(it);
        } else {
            ++it;
        }
    }
    std::size_t removed = 0;
    while (keptTexelsHeld > keptTexels && !kept.empty()) {
        const auto oldest = std::min_element(
            kept.begin(), kept.end(), [](const auto& a, const auto& b) { return a.second.left < b.second.left; });
        keptTexelsHeld -= texelsOf(*oldest->second.target);
        retired.add(std::move(oldest->second.target));
        kept.erase(oldest);
        ++removed;
    }
    return removed;
}

uint32_t TexturePool::sideOf(const UnwrappedTileID& id) const {
    if (const auto drawn = renderTargets.find(id); drawn != renderTargets.end() && drawn->second) {
        return drawn->second->getTexture()->getSize().width;
    }
    const auto back = kept.find(id);
    return back == kept.end() ? 0 : back->second.target->getTexture()->getSize().width;
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
