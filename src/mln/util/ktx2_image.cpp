#include <mln/util/ktx2_image.hpp>
#include <mln/util/premultiply.hpp>

#include <basisu_transcoder.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <utility>
#include <stdexcept>

namespace mln {
namespace util {

namespace {
std::atomic<bool> astc{false};

constexpr uint8_t ktx2Magic[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

void initTranscoder() {
    static std::once_flag once;
    std::call_once(once, [] { basist::basisu_transcoder_init(); });
}
} // namespace

bool isKTX2(const std::string& data) {
    return data.size() >= sizeof(ktx2Magic) && std::memcmp(data.data(), ktx2Magic, sizeof(ktx2Magic)) == 0;
}

void setASTCTextures(bool on) {
    astc = on;
}

bool astcTextures() {
    return astc;
}

bool astcTexturesFor(bool gpuDrawsASTC, const char* forcedRGBA) {
    return gpuDrawsASTC && !(forcedRGBA && std::strcmp(forcedRGBA, "1") == 0);
}

std::variant<PremultipliedImage, std::shared_ptr<ASTCImage>> decodeKTX2(const std::string& data, bool toASTC) {
    initTranscoder();
    basist::ktx2_transcoder ktx2;
    if (!ktx2.init(data.data(), static_cast<uint32_t>(data.size())) || !ktx2.start_transcoding()) {
        throw std::runtime_error("unreadable KTX2 image");
    }
    const Size size{ktx2.get_width(), ktx2.get_height()};
    // A state of the worker's own: the transcoder's shared one is not thread-safe.
    basist::ktx2_transcoder_state state;
    state.clear();

    const auto astcFormat =
        [&]() -> std::optional<std::pair<gfx::TexturePixelType, basist::transcoder_texture_format>> {
        using F = basist::transcoder_texture_format;
        if (ktx2.get_block_width() != ktx2.get_block_height()) return std::nullopt;
        switch (ktx2.get_block_width()) {
            case 4:
                return std::pair{gfx::TexturePixelType::ASTC4x4, F::cTFASTC_LDR_4x4_RGBA};
            case 6:
                return std::pair{gfx::TexturePixelType::ASTC6x6, F::cTFASTC_LDR_6x6_RGBA};
            case 8:
                return std::pair{gfx::TexturePixelType::ASTC8x8, F::cTFASTC_LDR_8x8_RGBA};
            default:
                return std::nullopt;
        }
    }();

    if (toASTC && astcFormat) {
        auto image = std::make_shared<ASTCImage>();
        image->size = size;
        image->format = astcFormat->first;
        const uint32_t side = gfx::astcBlockSide(image->format);
        const uint32_t blocks = ((size.width + side - 1) / side) * ((size.height + side - 1) / side);
        image->blocks.resize(std::size_t{blocks} * 16);
        if (!ktx2.transcode_image_level(
                0, 0, 0, image->blocks.data(), blocks, astcFormat->second, 0, 0, 0, -1, -1, &state)) {
            throw std::runtime_error("KTX2 image does not transcode to ASTC");
        }
        return image;
    }

    UnassociatedImage image(size);
    if (!ktx2.transcode_image_level(0,
                                    0,
                                    0,
                                    image.data.get(),
                                    size.width * size.height,
                                    basist::transcoder_texture_format::cTFRGBA32,
                                    0,
                                    0,
                                    0,
                                    -1,
                                    -1,
                                    &state)) {
        throw std::runtime_error("KTX2 image does not transcode to RGBA");
    }
    return util::premultiply(std::move(image));
}

} // namespace util
} // namespace mln
