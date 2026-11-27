#pragma once

#include <mln/tile/tile_id.hpp>
#include <mln/renderer/render_target.hpp>
#include <mln/util/retiring.hpp>
#include <optional>
#include <set>

namespace mln {
class TexturePool {
public:
    TexturePool(uint32_t tilesize);
    ~TexturePool();

    std::shared_ptr<RenderTarget> getRenderTarget(const UnwrappedTileID& id) const;
    std::shared_ptr<RenderTarget> getRenderTargetAncestorOrDescendant(
        const UnwrappedTileID& id, std::optional<UnwrappedTileID>& terrainTileID) const;
    /// `size` 0 is the pool's own; a target of another size is made anew.
    void createRenderTarget(gfx::Context& context,
                            const UnwrappedTileID& id,
                            const Color& backgroundColor,
                            uint32_t size = 0);

    /// Targets of tiles not in the set leave the pool at once, to be freed by `releaseRetired`.
    /// The targets whose tile left the cover go; returns how many.
    std::size_t removeStaleRenderTargets(const std::set<UnwrappedTileID>& currentTiles);

    /// Frees at most `count` targets that left the pool, every one when `count` is 0.
    std::size_t releaseRetired(std::size_t count) { return retired.release(count); }
    std::size_t retiredCount() const { return retired.size(); }

    template <typename Func /* void(std::shared_ptr<RenderTarget>&) */>
    void visitRenderTargets(Func f) {
        for (auto& pair : renderTargets) {
            if (pair.second) {
                f(pair.second);
            }
        }
    }

    template <typename Func /* void(std::shared_ptr<RenderTarget>&) */>
    void visitRenderTargets(Func f) const {
        for (const auto& pair : renderTargets) {
            if (pair.second) {
                f(pair.second);
            }
        }
    }

private:
    uint32_t tileSize;

    std::map<UnwrappedTileID, std::shared_ptr<RenderTarget>> renderTargets;
    util::Retiring<std::shared_ptr<RenderTarget>> retired;
};

} // namespace mln
