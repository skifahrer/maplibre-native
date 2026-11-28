#pragma once

#include <mln/util/image.hpp>
#include <mln/util/size.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mln {

/// GPU blocks of a 4 × 4 ASTC texture, 16 bytes a block, rows of blocks top to bottom.
struct ASTCImage {
    Size size;
    std::vector<uint8_t> blocks;

    std::size_t bytes() const { return blocks.size(); }
};

namespace util {

/// Whether a tile's bytes are a KTX2 file.
bool isKTX2(const std::string& data);

/// Whether the GPU draws ASTC 4 × 4 textures, set by the backend's context when it is made.
void setASTCTextures(bool);
bool astcTextures();

/// A KTX2 tile's first image: ASTC blocks when `astc`, else RGBA pixels; throws if it is not one.
std::variant<PremultipliedImage, std::shared_ptr<ASTCImage>> decodeKTX2(const std::string& data, bool astc);

} // namespace util
} // namespace mln
