#include <mln/geometry/hillshade_slopes.hpp>

#include <mln/geometry/dem_data.hpp>

#include <algorithm>
#include <cmath>

namespace mln {

namespace {

/// As hillshade_prepare.fragment.glsl: pixel-space Sobel sums to world slope, steeper at low zooms.
float slopeScale(const DEMData& dem, uint8_t zoom) {
    const auto z = static_cast<float>(zoom);
    const float factor = z < 2.0f ? 0.4f : (z < 4.5f ? 0.35f : 0.3f);
    const float exaggeration = z < 15.0f ? (z - 15.0f) * factor : 0.0f;
    return static_cast<float>(dem.dim) / std::pow(2.0f, exaggeration + (28.2562f - z));
}

uint8_t encoded(float slope) {
    return static_cast<uint8_t>(std::lround(std::clamp(slope / 8.0f + 0.5f, 0.0f, 1.0f) * 255.0f));
}

struct Sampler {
    const DEMData& dem;
    const uint8_t* pixels;
    const std::array<float, 4>& unpack;
    float scale;

    // The shader decodes every texel as it is, no-data included, so this does too.
    float height(int32_t x, int32_t y) const {
        const uint8_t* value = pixels + (static_cast<size_t>(y + 2) * dem.stride + (x + 2)) * 4;
        return value[0] * unpack[0] + value[1] * unpack[1] + value[2] * unpack[2] - unpack[3];
    }

    void write(PremultipliedImage& slopes, uint32_t column, uint32_t row) const {
        const auto x = static_cast<int32_t>(column) - 1, y = static_cast<int32_t>(row) - 1;
        const float a = height(x - 1, y - 1), b = height(x, y - 1), c = height(x + 1, y - 1);
        const float d = height(x - 1, y), f = height(x + 1, y);
        const float g = height(x - 1, y + 1), h = height(x, y + 1), i = height(x + 1, y + 1);
        uint8_t* out = slopes.data.get() + (static_cast<size_t>(row) * slopes.size.width + column) * 4;
        out[0] = encoded(((c + f + f + i) - (a + d + d + g)) * scale);
        out[1] = encoded(((g + h + h + i) - (a + b + b + c)) * scale);
        out[2] = 255;
        out[3] = 255;
    }
};

Sampler samplerOf(const DEMData& dem, uint8_t zoom) {
    return {dem, dem.getImage()->data.get(), dem.getUnpackVector(), slopeScale(dem, zoom)};
}

} // namespace

PremultipliedImage HillshadeSlopes::make(const DEMData& dem, uint8_t zoom) {
    const auto side = static_cast<uint32_t>(dem.dim + 2);
    PremultipliedImage slopes({side, side});
    const auto sampler = samplerOf(dem, zoom);
    for (uint32_t row = 0; row < side; row++) {
        for (uint32_t column = 0; column < side; column++) {
            sampler.write(slopes, column, row);
        }
    }
    return slopes;
}

void HillshadeSlopes::remakeEdges(PremultipliedImage& slopes, const DEMData& dem, uint8_t zoom) {
    const auto side = static_cast<uint32_t>(dem.dim + 2);
    if (slopes.size.width != side || slopes.size.height != side) {
        slopes = make(dem, zoom);
        return;
    }
    const auto sampler = samplerOf(dem, zoom);
    for (uint32_t along = 0; along < side; along++) {
        for (const uint32_t across : {0u, 1u, side - 2, side - 1}) {
            sampler.write(slopes, along, across);
            sampler.write(slopes, across, along);
        }
    }
}

} // namespace mln
