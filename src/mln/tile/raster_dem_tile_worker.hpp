#pragma once

#include <mln/actor/actor_ref.hpp>
#include <mln/util/tileset.hpp>

#include <memory>
#include <string>

namespace mln {

class RasterDEMTile;

class RasterDEMTileWorker {
public:
    RasterDEMTileWorker(const ActorRef<RasterDEMTileWorker>&, ActorRef<RasterDEMTile>);

    void parse(const std::shared_ptr<const std::string>& data,
               uint64_t correlationID,
               Tileset::RasterEncoding encoding,
               uint8_t zoom);

private:
    ActorRef<RasterDEMTile> parent;
};

} // namespace mln
