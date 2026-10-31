#pragma once

#include <mln/shaders/terrain_layer_ubo.hpp>
#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/mtl/shader_program.hpp>

namespace mln {
namespace shaders {

constexpr auto terrainShaderPrelude = R"(

enum {
    idTerrainDrawableUBO = idDrawableReservedVertexOnlyUBO,
    idTerrainTilePropsUBO = idDrawableReservedFragmentOnlyUBO,
    idTerrainEvaluatedPropsUBO = drawableReservedUBOCount,
    terrainUBOCount
};

struct alignas(16) TerrainDrawableUBO {
    /*  0 */ float4x4 matrix;
    /* 64 */ float4 dem_coords;
    /* 80 */
};
static_assert(sizeof(TerrainDrawableUBO) == 5 * 16, "wrong size");

struct alignas(16) TerrainTilePropsUBO {
    /*  0 */ float2 dem_tl;
    /*  8 */ float dem_scale;
    /* 12 */ float pad1;
    /* 16 */
};
static_assert(sizeof(TerrainTilePropsUBO) == 16, "wrong size");

/// Evaluated properties that do not depend on the tile
struct alignas(16) TerrainEvaluatedPropsUBO {
    /*  0 */ float4 unpack; // DEM unpack vector for the source's encoding
    /* 16 */ float exaggeration;
    /* 20 */ float elevation_offset;
    /* 24 */ float no_ground;
    /* 28 */ float cut_ndc_x;
    /* 32 */ float4 fog_color;
    /* 48 */ float fog_start;
    /* 52 */ float fog_end;
    /* 56 */ float cut_depth;
    /* 60 */ float cut_ndc_y;
    /* 64 */
};
static_assert(sizeof(TerrainEvaluatedPropsUBO) == 64, "wrong size");

// Terrain the camera must see through: what stands nearer than the centre in a window over it,
// so the mountains to the sides stay. The curtains under the cut tiles' edges go with it.
inline bool cut_away(device const TerrainEvaluatedPropsUBO& props, float3 clip_xyw, float depth, float skirt) {
    if (props.cut_depth <= 0.0) {
        return false;
    }
    // Deepest over the centre, none at the window's edge: the cut follows the ground, no seam.
    const float2 ndc = clip_xyw.xy / clip_xyw.z;
    const float2 off = (ndc - float2(props.cut_ndc_x, props.cut_ndc_y)) / float2(0.5, ndc.y > props.cut_ndc_y ? 1.0 : 0.3);
    const float reach = 1.0 - dot(off, off);
    return reach > 0.0 && (depth < props.cut_depth * reach || skirt > 0.5);
}

)";

template <>
struct ShaderSource<BuiltIn::TerrainShader, gfx::Backend::Type::Metal> {
    static constexpr auto name = "TerrainShader";
    static constexpr auto vertexMainFunction = "vertexMain";
    static constexpr auto fragmentMainFunction = "fragmentMain";

    static const std::array<AttributeInfo, 1> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 2> textures;

    static constexpr auto prelude = terrainShaderPrelude;
    static constexpr auto source = R"(

struct VertexStage {
    short4 pos [[attribute(0)]]; // xy = tile position, z = skirt flag (1 = skirt)
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float2 uv;
    float elevation;
    float depth; // view depth, for the fog
    float3 clip_xyw; // clip y and w, for the cutaway in front of the centre
    float skirt;    // 1 on the curtain below a tile edge
    float ground;   // 1 where the height model has ground, 0 where it has none
    float none;     // 1 at a vertex with no ground under it at all
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const uint32_t& uboIndex [[buffer(idGlobalUBOIndex)]],
                                device const TerrainDrawableUBO* drawableVector [[buffer(idTerrainDrawableUBO)]],
                                device const TerrainEvaluatedPropsUBO& props [[buffer(idTerrainEvaluatedPropsUBO)]],
                                texture2d<float, access::sample> demTexture [[texture(0)]],
                                sampler demSampler [[sampler(0)]]) {

    device const TerrainDrawableUBO& drawable = drawableVector[uboIndex];

    // The mesh was generated with coordinates from 0 to EXTENT (8192)
    float2 pos = float2(vertx.pos.xy);
    float2 uv = pos / 8192.0;

    // Decode the DEM and interpolate in meters via the shared helper (the packed
    // Terrain-RGB/Terrarium DEM cannot be hardware-filtered, so it is sampled
    // NEAREST and interpolated after decoding, matching maplibre-gl-js and the
    // elevated layers). Map into the bound DEM tile; an ancestor tile is bound as
    // a fallback while this tile's own DEM loads. dem_coords.w = DEM dimension.
    float ground = 1.0;
    float elevation = get_ground_elevation(pos, demTexture, drawable.dem_coords, props.unpack,
                                           drawable.dem_coords.w, props.exaggeration, props.no_ground, ground);
    if (vertx.pos.z == 1 && !ground_throughout(demTexture, drawable.dem_coords, drawable.dem_coords.w,
                                                props.unpack, props.no_ground)) {
        ground = 0.0;
    }

    // Skirt vertices hang below the surface by elevation_offset, forming a curtain
    // that hides the cracks between neighbouring tiles at different zoom levels
    // (maplibre-gl-js u_ele_delta). pos.z carries the skirt flag.
    const float ele_delta = (float(vertx.pos.z) == 1.0) ? props.elevation_offset : 0.0;
    float4 position = drawable.matrix * float4(pos.x, pos.y, elevation - ele_delta, 1.0);

    return {
        .position  = position,
        .uv        = uv,
        .elevation = elevation,
        .depth     = position.w,
        .clip_xyw  = position.xyw,
        .skirt     = float(vertx.pos.z),
        .ground    = ground,
        .none      = ground < 0.001 ? 1.0 : 0.0,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const TerrainEvaluatedPropsUBO& props [[buffer(idTerrainEvaluatedPropsUBO)]],
                            texture2d<float, access::sample> mapTexture [[texture(1)]],
                            sampler mapSampler [[sampler(1)]]) {
#if defined(OVERDRAW_INSPECTOR)
    return half4(1.0);
#endif

    // Sample the map texture (render-to-texture output) for the surface color.
    // The drape RTT is rendered with the core's GL-convention projection. On Vulkan
    // (NDC Y-down) that stores the RTT already flipped, so its terrain shader samples
    // 1.0 - uv.y. Metal's NDC is Y-up, so the RTT is stored upright and the V must NOT
    // be flipped here - sampling 1.0 - uv.y flips the draped map vertically (north/south
    // swapped about the view). Sample uv.y directly on Metal.
    if (cut_away(props, in.clip_xyw, in.depth, in.skirt)) {
        discard_fragment();
    }
    // The curtains under ground the model does not have
    if (in.skirt > 0.5 && in.ground < 0.999) {
        discard_fragment();
    }
    float4 color = on_clay_face(in.none) ? clay_color(in.elevation)
                                         : mapTexture.sample(mapSampler, float2(in.uv.x, in.uv.y));

    // Fog: from fog_start to fog_end the surface fades into the fog colour, straight (not
    // premultiplied) and opaque, by as much as the colour's alpha.
    if (props.fog_color.a > 0.0) {
        const float t = smoothstep(props.fog_start, max(props.fog_end, props.fog_start + 1.0), in.depth);
        const float4 fog = float4(props.fog_color.rgb / props.fog_color.a, 1.0);
        color = mix(color, fog, t * props.fog_color.a);
    }
    return half4(color);
}
)";
};

} // namespace shaders
} // namespace mln
