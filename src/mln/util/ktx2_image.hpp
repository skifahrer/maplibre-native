#pragma once

#include <mln/gfx/types.hpp>
#include <mln/util/image.hpp>
#include <mln/util/size.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mln {

/// GPU blocks of an ASTC texture, 16 bytes a block, rows of blocks top to bottom.
struct ASTCImage {
    Size size;
    gfx::TexturePixelType format = gfx::TexturePixelType::ASTC4x4;
    std::vector<uint8_t> blocks;

    std::size_t bytes() const { return blocks.size(); }
};

namespace util {

/// Whether a tile's bytes are a KTX2 file.
bool isKTX2(const std::string& data);

/// Whether the GPU draws ASTC textures, set by the backend's context when it is made.
void setASTCTextures(bool);
bool astcTextures();

/// A KTX2 tile's image: ASTC blocks of its own size when `astc`, else RGBA; throws if none.
std::variant<PremultipliedImage, std::shared_ptr<ASTCImage>> decodeKTX2(const std::string& data, bool astc);

} // namespace util
} // namespace mln
