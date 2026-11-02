#pragma once

namespace mln {
namespace shaders {

constexpr auto prelude = R"(
#include <metal_stdlib>
using namespace metal;

// The maximum allowed miter limit is 2.0 at the moment. the extrude normal is stored
// in a byte (-128..127). We scale regular normals up to length 63, but there are also
// "special" normals that have a bigger length (of up to 126 in this case).
#define LINE_NORMAL_SCALE (1.0 / (127 / 2))

// The attribute conveying progress along a line is scaled to [0, 2^15).
#define MAX_LINE_DISTANCE 32767.0

#define SDF_PX 8.0

// OpenGL `mod` is `x-y*floor(x/y)` where `floor` rounds down.
// Metal `fmod` is `x-y*trunc(x/y)` where `trunc` rounds toward zero.
// This function provides GL-compatible modulus for porting GLSL shaders.
template <typename T1, typename T2>
inline auto glMod(T1 x, T2 y) { return x - y * metal::floor(x/y); }

inline float radians(float degrees) {
    return M_PI_F * degrees / 180.0;
}

// Unpack a pair of values that have been packed into a single float.
// The packed values are assumed to be 8-bit unsigned integers, and are
// packed like so: packedValue = floor(input[0]) * 256 + input[1],
float2 unpack_float(const float packedValue) {
    const int packedIntValue = int(packedValue);
    const int v0 = packedIntValue / 256;
    return float2(v0, packedIntValue - v0 * 256);
}
float2 unpack_opacity(const float packedOpacity) {
    return float2(float(int(packedOpacity) / 2) / 127.0, glMod(packedOpacity, 2.0));
}
// To minimize the number of attributes needed, we encode a 4-component
// color into a pair of floats (i.e. a vec2) as follows:
// [ floor(color.r * 255) * 256 + color.g * 255, floor(color.b * 255) * 256 + color.a * 255 ]
float4 decode_color(const float2 encoded) {
    return float4(unpack_float(encoded[0]) / 255, unpack_float(encoded[1]) / 255);
}
// Unpack a pair of paint values and interpolate between them.
float unpack_mix_float(device const float packedValue[2], const float t) {
    return mix(packedValue[0], packedValue[1], t);
}
float unpack_mix_float(const float2 packedValue, const float t) {
    return mix(packedValue[0], packedValue[1], t);
}
// Unpack a pair of paint values and interpolate between them.
float4 unpack_mix_color(device const float packedColors[4], const float t) {
    return mix(decode_color(float2(packedColors[0], packedColors[1])),
               decode_color(float2(packedColors[2], packedColors[3])), t);
}
float4 unpack_mix_color(const float4 packedColors, const float t) {
    return mix(decode_color(float2(packedColors[0], packedColors[1])),
               decode_color(float2(packedColors[2], packedColors[3])), t);
}

// Unpack pattern position
inline float2 get_pattern_pos(const float2 pixel_coord_upper, const float2 pixel_coord_lower,
                     const float2 pattern_size, const float tile_units_to_pixels, const float2 pos) {
    const float2 offset = glMod(glMod(glMod(pixel_coord_upper, pattern_size) * 256.0, pattern_size) * 256.0 + pixel_coord_lower, pattern_size);
    return (tile_units_to_pixels * pos + offset) / pattern_size;
}

// Sample the terrain elevation in meters at a tile-local coordinate, with manual
// bilinear interpolation on DEM pixel centers (the DEM has a 2px backfilled border),
// matching the maplibre-gl-js get_elevation() prelude function. Unlike gl-js (global
// terrain uniforms), MapLibre Native carries the DEM data per-drawable, so the DEM
// texture and dem_* values are passed in as arguments rather than read from globals.
// Decode one DEM texel to meters. The clamp at the call sites matters: read() out of range
// is undefined where the sampler's clamp-to-edge used to cover it, and a tile with no DEM of
// its own is bound the 1x1 placeholder, whose only valid texel is (0, 0).
inline float dem_texel(texture2d<float, access::sample> dem, int2 texel, float4 dem_unpack) {
    float4 data = dem.read(uint2(texel)) * 255.0;
    data.a = -1.0;
    return dot(data, dem_unpack);
}

inline float get_ground_elevation(float2 pos,
                                  texture2d<float, access::sample> dem,
                                  float4 dem_coords,
                                  float4 dem_unpack,
                                  float dem_dim,
                                  float dem_exaggeration,
                                  float no_ground,
                                  thread float& ground);

// Where the model has no ground (dem_coords.w, its plane), at sea level, as the terrain draws it
inline float get_elevation(float2 pos,
                           texture2d<float, access::sample> dem,
                           sampler dem_sampler,
                           float4 dem_coords,
                           float4 dem_unpack,
                           float dem_dim,
                           float dem_exaggeration,
                           float dem_enabled) {
    if (dem_enabled == 0.0) {
        return 0.0;
    }
    float ground = 1.0;
    return get_ground_elevation(pos, dem, dem_coords, dem_unpack, dem_dim, dem_exaggeration, dem_coords.w, ground);
}

// Ground, unless the DEM's height there is the plane it writes where it has none: exact in
// Terrarium's 1/256 m steps, and no real surface holds one height over the whole area.
inline bool dem_has_ground(float height, float no_ground) {
    return abs(height - no_ground) > 0.002;
}

// The elevation, and how much of the vertex stands on the model's ground (`ground`, 0 to 1).
// No ground lies at sea level: a triangle joining it to ground is the clay face of the model.
inline float get_ground_elevation(float2 pos,
                                  texture2d<float, access::sample> dem,
                                  float4 dem_coords,
                                  float4 dem_unpack,
                                  float dem_dim,
                                  float dem_exaggeration,
                                  float no_ground,
                                  thread float& ground) {
    const float2 coord = (pos * dem_coords.x + dem_coords.yz) * dem_dim + 2.0;
    const float2 f = fract(coord);
    const int2 c = int2(floor(coord));
    const int2 hi = int2(dem.get_width(), dem.get_height()) - 1;
    const int2 taps[4] = {c, c + int2(1, 0), c + int2(0, 1), c + int2(1, 1)};
    const float weights[4] = {(1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y};
    float on = 0.0;
    float onWeight = 0.0;
    float offWeight = 0.0;
    for (int i = 0; i < 4; i++) {
        const float height = dem_texel(dem, clamp(taps[i], int2(0), hi), dem_unpack);
        if (dem_has_ground(height, no_ground)) {
            on += weights[i] * height;
            onWeight += weights[i];
        } else {
            offWeight += weights[i];
        }
    }
    ground = onWeight / max(onWeight + offWeight, 1e-6);
    const float elevation = onWeight > 0.0 ? on / onWeight : 0.0;
    return elevation * dem_exaggeration;
}

// A fragment of a triangle from ground down to none (`none` is 1 at a vertex with no ground)
inline bool on_clay_face(float none) {
    return none > 0.001 && none < 0.999;
}

// Clay grey, or the map's own (premultiplied), darker toward the table it stands on
inline float4 clay_color(float elevation, float4 chosen) {
    const float3 clay = chosen.a > 0.0 ? chosen.rgb / chosen.a : float3(0.58, 0.56, 0.53);
    return float4(clay * mix(0.62, 1.0, saturate(elevation / 1500.0)), 1.0);
}

// Whether the tile has ground throughout, on a 9 by 9 grid. A tile edge's curtain hangs only
// in such a tile: one that reaches the model's edge has a neighbour without ground, at another
// zoom too (whose heights never reach this tile's border), and its curtain stood as a wall.
inline bool ground_throughout(texture2d<float, access::sample> dem, float4 dem_coords, float dem_dim,
                              float4 dem_unpack, float no_ground) {
    // The 1x1 placeholder: no DEM here, past the model's tiles, so nothing for a curtain to join
    if (dem.get_width() < 2) {
        return false;
    }
    const int2 hi = int2(dem.get_width(), dem.get_height()) - 1;
    for (int y = 0; y <= 8; y++) {
        for (int x = 0; x <= 8; x++) {
            const float2 pos = float2(x, y) * 1024.0;
            const int2 c = int2(floor((pos * dem_coords.x + dem_coords.yz) * dem_dim + 2.0));
            if (!dem_has_ground(dem_texel(dem, clamp(c, int2(0), hi), dem_unpack), no_ground)) {
                return false;
            }
        }
    }
    return true;
}

// The camera distance (clip w) the terrain depth pass packed (mtl/terrain_depth.hpp)
inline float unpack_distance(float4 rgba_depth) {
    const float4 bit_shift = float4(1.0 / (256.0 * 256.0 * 256.0), 1.0 / (256.0 * 256.0), 1.0 / 256.0, 1.0);
    return dot(rgba_depth, bit_shift) * 16777216.0;
}

// Opacity of a clip-space position behind the terrain, in [0, 1]: 1 fully visible, 0
// fully hidden. It compares distances from the camera, not NDC z: with the near plane
// at 1 unit, a ridge and the valley behind it differ in z by less than any fixed bias.
// Hidden from 3% farther than the terrain, fully at 10%: the mesh is coarser than the
// DEM a label or circle samples, and bridges the gullies they sit in.
// Where no terrain is drawn (past the draw distance, or past the nearest tiles a load mode
// keeps) the depth target holds its white clear colour: `sky` is the opacity there.
inline float depth_opacity(float4 pos,
                           texture2d<float, access::sample> depth_texture,
                           sampler depth_sampler,
                           float sky) {
    const float2 uv = pos.xy / pos.w * 0.5 + 0.5;
    const float terrain = unpack_distance(depth_texture.sample(depth_sampler, float2(uv.x, 1.0 - uv.y)));
    if (terrain >= 16777216.0) {
        return sky;
    }
    const float behind = pos.w / terrain - 1.0;
    return 1.0 - clamp((behind - 0.03) / 0.07, 0.0, 1.0);
}

// A label or circle over no terrain stands on ground that is not drawn, so it would float in
// the sky above the horizon: hidden.
inline float depth_opacity(float4 pos,
                           texture2d<float, access::sample> depth_texture,
                           sampler depth_sampler) {
    return depth_opacity(pos, depth_texture, depth_sampler, 0.0);
}

// Whether a clip-space position is visible in front of the terrain, from the packed
// terrain depth texture. The depth texture and enable flag are passed as arguments.
inline float calculate_visibility(float4 pos,
                                  texture2d<float, access::sample> depth_texture,
                                  sampler depth_sampler,
                                  float depth_enabled) {
    if (depth_enabled == 0.0) {
        return 1.0;
    }
    // Behind nearer ground it is hidden: a name hangs below its anchor, nothing pokes up.
    const float d = depth_opacity(pos, depth_texture, depth_sampler, -1.0);
    if (d >= 0.0) {
        return d;
    }
    // Over no terrain: a summit above the coarser mesh shows, ground not drawn does not.
    return depth_opacity(pos - float4(0.0, 0.01 * pos.w, 0.0, 0.0), depth_texture, depth_sampler, 0.0);
}

template<class ForwardIt, class T>
ForwardIt upper_bound(ForwardIt first, ForwardIt last, thread const T& value)
{
    size_t count = last - first;
    while (count > 0)
    {
        ForwardIt it = first;
        const size_t step = count / 2;
        it += step;
        if (!(value < *it))
        {
            first = ++it;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return first;
}

float interpolationFactor(float base, float rangeMin, float rangeMax, float z) {
    const float zoomDiff = rangeMax - rangeMin;
    const float zoomProgress = z - rangeMin;
    if (zoomDiff == 0) {
        return 0;
    } else if (base == 1.0f) {
        return zoomProgress / zoomDiff;
    } else {
        return (pow(base, zoomProgress) - 1) / (pow(base, zoomDiff) - 1);
    }
}


enum class GPUInterpType : uint16_t {
    Step,
    Linear,
    Exponential,
    Bezier
};
enum class GPUOutputType : uint16_t {
    Float,
    Color,
};
enum class GPUOptions : uint16_t {
    None = 0,
    IntegerZoom = 1 << 0,
    Transitioning = 1 << 1,
};
bool operator&(GPUOptions a, GPUOptions b) { return (uint16_t)a & (uint16_t)b; }

constant const int maxExprStops = 16;
struct alignas(16) GPUExpression {
    GPUOutputType outputType;
    uint16_t stopCount;
    GPUOptions options;
    GPUInterpType interpolation;

    union InterpOptions {
        struct Exponential {
            float base;
        } exponential;

        struct Bezier {
            float x1;
            float y1;
            float x2;
            float y2;
        } bezier;
    } interpOptions;

    float inputs[maxExprStops];

    union Stops {
        float floats[maxExprStops];
        float2 colors[maxExprStops];
    } stops;

    float eval(float zoom) device const {
        const auto effectiveZoom = (options & GPUOptions::IntegerZoom) ? floor(zoom) : zoom;
        const auto index = find(effectiveZoom);
        if (index == 0) {
            return stops.floats[0];
        } else if (index == stopCount) {
            return stops.floats[stopCount - 1];
        }
        switch (interpolation) {
            case GPUInterpType::Step: return stops.floats[index - 1];
            default: assert(false);
                [[fallthrough]];
            case GPUInterpType::Linear: assert(interpOptions.exponential.base == 1.0f);
                [[fallthrough]];
            case GPUInterpType::Exponential: {
                const float rangeBeg = inputs[index - 1];
                const float rangeEnd = inputs[index];
                const auto t = interpolationFactor(interpOptions.exponential.base, rangeBeg, rangeEnd, effectiveZoom);
                return mix(stops.floats[index - 1], stops.floats[index], t);
            }
            case GPUInterpType::Bezier:
                assert(false);
                return stops.floats[0];
        }
    }

    float4 evalColor(float zoom) device const {
        const auto effectiveZoom = (options & GPUOptions::IntegerZoom) ? floor(zoom) : zoom;
        const auto index = find(effectiveZoom);
        if (index == 0) {
            return getColor(0);
        } else if (index == stopCount) {
            return getColor(stopCount - 1);
        }
        switch (interpolation) {
            case GPUInterpType::Step:
                return getColor(index - 1);
            default:
                assert(false);
                [[fallthrough]];
            case GPUInterpType::Linear:
                assert(interpOptions.exponential.base == 1.0f);
                [[fallthrough]];
            case GPUInterpType::Exponential: {
                const float rangeBeg = inputs[index - 1];
                const float rangeEnd = inputs[index];
                const auto t = interpolationFactor(interpOptions.exponential.base, rangeBeg, rangeEnd, effectiveZoom);
                return mix(getColor(index - 1), getColor(index), clamp(t, 0.0, 1.0));
            }
            case GPUInterpType::Bezier:
                assert(false);
                return getColor(0);
        }
    }

    /// Get the index of the entry to use from the zoom level
    size_t find(float zoom) device const {
        return upper_bound(&inputs[0], &inputs[stopCount], zoom) - &inputs[0];
    }

    float4 getColor(size_t index) device const { return decode_color(stops.colors[index]); }
};
static_assert(sizeof(GPUExpression) == 32 + (4 + 8) * maxExprStops, "wrong size");
static_assert(sizeof(GPUExpression) % 16 == 0, "wrong alignment");

struct alignas(16) GlobalPaintParamsUBO {
    /*  0 */ float2 pattern_atlas_texsize;
    /*  8 */ float2 units_to_pixels;
    /* 16 */ float2 world_size;
    /* 24 */ float camera_to_center_distance;
    /* 28 */ float symbol_fade_change;
    /* 32 */ float aspect_ratio;
    /* 36 */ float pixel_ratio;
    /* 40 */ float map_zoom;
    /* 44 */ float pad1;
    /* 48 */ float4 drape_tile;
    /* 64 */
};
static_assert(sizeof(GlobalPaintParamsUBO) == 4 * 16, "wrong size");

// Place a clip-space position computed with a tile-local drape matrix into the
// current terrain drape render target (see the GL prelude for the derivation).
// `matrix` carries the drawable's tile (z, x, y) in its unused third column and
// target_tile is GlobalPaintParamsUBO::drape_tile (w != 0 while drawing into a
// drape target).
inline float4 apply_drape_transform(float4 clip, float4x4 matrix, float4 target_tile) {
    if (target_tile.w == 0.0) {
        return clip;
    }
    const float3 tile = float3(matrix[2][0], matrix[2][1], matrix[2][2]);
    const float k = target_tile.x - tile.x; // target zoom - drawable zoom
    const float scale = exp2(k);
    float2 offset;
    if (k >= 0.0) {
        offset = tile.yz * scale - target_tile.yz;
    } else {
        offset = (tile.yz - target_tile.yz * exp2(-k)) * scale;
    }
    clip.x = clip.x * scale + (scale - 1.0 + 2.0 * offset.x);
    clip.y = clip.y * scale + (1.0 - scale - 2.0 * offset.y);
    return clip;
}

enum {
    idGlobalPaintParamsUBO,
    idGlobalUBOIndex,
    globalUBOCount,
};

enum {
    idDrawableReservedVertexOnlyUBO = globalUBOCount,
    idDrawableReservedFragmentOnlyUBO,
    drawableReservedUBOCount
};

)";

}
} // namespace mln
