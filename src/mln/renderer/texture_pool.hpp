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

    /// Targets of tiles not in the set leave the cover: the latest to leave are kept while they
    /// fit `keptTexels`, the rest go to be freed by `releaseRetired`; returns how many go.
    std::size_t removeStaleRenderTargets(const std::set<UnwrappedTileID>& currentTiles, std::size_t keptTexels = 0);
    std::size_t keptCount() const { return kept.size(); }
    /// The side of the tile's target, drawn or kept; 0 when it has none.
    uint32_t sideOf(const UnwrappedTileID& id) const;

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

    struct Kept {
        std::shared_ptr<RenderTarget> target;
        uint64_t left;
    };

    std::map<UnwrappedTileID, std::shared_ptr<RenderTarget>> renderTargets;
    std::map<UnwrappedTileID, Kept> kept;
    std::size_t keptTexelsHeld = 0;
    uint64_t departures = 0;
    util::Retiring<std::shared_ptr<RenderTarget>> retired;
};

} // namespace mln
