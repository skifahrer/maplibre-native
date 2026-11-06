#pragma once

#include <mln/tile/tile_id.hpp>
#include <mln/renderer/render_target.hpp>
#include <optional>
#include <set>
#include <vector>

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

    /// Remove render targets for tiles that are no longer part of the given set. Their
    /// textures wait a frame for a new target of the same size; an empty set frees them.
    void removeStaleRenderTargets(const std::set<UnwrappedTileID>& currentTiles);

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
    void retire(std::shared_ptr<RenderTarget> target);
    std::shared_ptr<RenderTarget> reuse(gfx::Context& context, uint32_t side);

    uint32_t tileSize;

    std::map<UnwrappedTileID, std::shared_ptr<RenderTarget>> renderTargets;

    struct Retired {
        std::shared_ptr<RenderTarget> target;
        uint64_t frame;
    };
    std::vector<Retired> retired;
    uint64_t frame = 0;
};

} // namespace mln
