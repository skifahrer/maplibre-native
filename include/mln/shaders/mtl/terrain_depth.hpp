#pragma once

#include <mln/shaders/terrain_layer_ubo.hpp>
#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/mtl/shader_program.hpp>
#include <mln/shaders/mtl/terrain.hpp>

namespace mln {
namespace shaders {

template <>
struct ShaderSource<BuiltIn::TerrainDepthShader, gfx::Backend::Type::Metal> {
    static constexpr auto name = "TerrainDepthShader";
    static constexpr auto vertexMainFunction = "vertexMain";
    static constexpr auto fragmentMainFunction = "fragmentMain";

    static const std::array<AttributeInfo, 1> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 1> textures;

    // Shares the terrain shader's UBO layout (same drawable/props buffers)
    static constexpr auto prelude = terrainShaderPrelude;
    static constexpr auto source = R"(

struct VertexStage {
    short4 pos [[attribute(0)]]; // xy = tile position, z = skirt flag (1 = skirt)
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float distance;
    float3 clip_xyw;
    float skirt;
    float ground;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const uint32_t& uboIndex [[buffer(idGlobalUBOIndex)]],
                                device const TerrainDrawableUBO* drawableVector [[buffer(idTerrainDrawableUBO)]],
                                device const TerrainEvaluatedPropsUBO& props [[buffer(idTerrainEvaluatedPropsUBO)]],
                                texture2d<float, access::sample> demTexture [[texture(0)]],
                                sampler demSampler [[sampler(0)]]) {

    device const TerrainDrawableUBO& drawable = drawableVector[uboIndex];

    // Same elevation displacement as the terrain shader (mtl/terrain.hpp),
    // rendering only depth for the symbol occlusion pass
    const float2 pos = float2(vertx.pos.xy);

    const float dem_size = floor(drawable.dem_coords.w);
    float ground = 1.0;
    const float elevation = get_ground_elevation(pos, demTexture, drawable.dem_coords, props.unpack,
                                                 dem_size, props.exaggeration, props.no_ground, ground);
    if (vertx.pos.z == 1 && !ground_throughout(demTexture, drawable.dem_coords, dem_size,
                                                props.unpack, props.no_ground)) {
        ground = 0.0;
    }

    // Skirt vertices drop below the surface by elevation_offset (gl-js u_ele_delta)
    const float ele_delta = (float(vertx.pos.z) == 1.0 && curtain_hangs(pos, drawable.dem_coords.w))
                                ? props.elevation_offset
                                : 0.0;
    const float4 position = drawable.matrix * float4(pos.x, pos.y, elevation - ele_delta, 1.0);
    return {
        .position = position,
        .distance = position.w,
        .clip_xyw = position.xyw,
        .skirt    = float(vertx.pos.z),
        .ground   = ground,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]],
                            device const TerrainEvaluatedPropsUBO& props [[buffer(idTerrainEvaluatedPropsUBO)]]) {
    // The cutaway of the terrain shader, so labels behind the cut mountain show
    if (cut_away(props, in.clip_xyw, in.distance, in.skirt)) {
        discard_fragment();
    }
    // The curtains under ground the model does not have
    if (in.skirt > 0.5 && in.ground < 0.999) {
        discard_fragment();
    }
    // The camera distance / 2^24 as four base-256 digits, each k / 255 so the unorm8 target
    // keeps it exactly; unpack_distance() reads it back. The white clear colour is the sky.
    float r = min(in.distance / 16777216.0, 1.0 - 1.0 / 65536.0) * 256.0;
    const float d0 = floor(r);
    r = (r - d0) * 256.0;
    const float d1 = floor(r);
    r = (r - d1) * 256.0;
    const float d2 = floor(r);
    r = (r - d2) * 256.0;
    const float d3 = floor(r);
    return half4(float4(d0, d1, d2, d3) / 255.0);
}
)";
};

} // namespace shaders
} // namespace mln
