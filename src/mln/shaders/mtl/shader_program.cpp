#include <mln/shaders/mtl/shader_program.hpp>
#include <mln/util/hash.hpp>

#include <mln/gfx/render_pass.hpp>
#include <mln/mtl/context.hpp>
#include <mln/mtl/renderer_backend.hpp>
#include <mln/mtl/renderable_resource.hpp>
#include <mln/mtl/uniform_buffer.hpp>
#include <mln/mtl/vertex_attribute.hpp>
#include <mln/shaders/program_parameters.hpp>
#include <mln/shaders/shader_manifest.hpp>
#include <mln/util/logging.hpp>

#include <Metal/MTLLibrary.hpp>
#include <Metal/MTLRenderPass.hpp>
#include <Metal/MTLRenderPipeline.hpp>

#include <cstring>
#include <utility>
#include <algorithm>

using namespace std::string_literals;

namespace mln {

namespace mtl {
namespace {
MTL::BlendOperation metalBlendOperation(const gfx::ColorBlendEquationType& colorBlend) {
    switch (colorBlend) {
        case gfx::ColorBlendEquationType::Add:
            return MTL::BlendOperationAdd;
        case gfx::ColorBlendEquationType::Subtract:
            return MTL::BlendOperationSubtract;
        case gfx::ColorBlendEquationType::ReverseSubtract:
            return MTL::BlendOperationReverseSubtract;
    }
}

MTL::BlendFactor metalBlendFactor(const gfx::ColorBlendFactorType& colorFactor) {
    switch (colorFactor) {
        case gfx::ColorBlendFactorType::Zero:
            return MTL::BlendFactorZero;
        case gfx::ColorBlendFactorType::One:
            return MTL::BlendFactorOne;
        case gfx::ColorBlendFactorType::SrcColor:
            return MTL::BlendFactorSourceColor;
        case gfx::ColorBlendFactorType::OneMinusSrcColor:
            return MTL::BlendFactorOneMinusSourceColor;
        case gfx::ColorBlendFactorType::SrcAlpha:
            return MTL::BlendFactorSourceAlpha;
        case gfx::ColorBlendFactorType::OneMinusSrcAlpha:
            return MTL::BlendFactorOneMinusSourceAlpha;
        case gfx::ColorBlendFactorType::DstAlpha:
            return MTL::BlendFactorDestinationAlpha;
        case gfx::ColorBlendFactorType::OneMinusDstAlpha:
            return MTL::BlendFactorOneMinusDestinationAlpha;
        case gfx::ColorBlendFactorType::DstColor:
            return MTL::BlendFactorDestinationColor;
        case gfx::ColorBlendFactorType::OneMinusDstColor:
            return MTL::BlendFactorOneMinusDestinationColor;
        case gfx::ColorBlendFactorType::SrcAlphaSaturate:
            return MTL::BlendFactorSourceAlphaSaturated;
        case gfx::ColorBlendFactorType::ConstantColor:
            return MTL::BlendFactorBlendColor;
        case gfx::ColorBlendFactorType::OneMinusConstantColor:
            return MTL::BlendFactorOneMinusBlendColor;
        case gfx::ColorBlendFactorType::ConstantAlpha:
            return MTL::BlendFactorBlendAlpha;
        case gfx::ColorBlendFactorType::OneMinusConstantAlpha:
            return MTL::BlendFactorOneMinusBlendAlpha;
    }
}
} // namespace

ShaderProgram::ShaderProgram(std::string name, RendererBackend& backend_, MTLFunctionPtr vert, MTLFunctionPtr frag)
    : ShaderProgramBase(),
      shaderName(std::move(name)),
      backend(backend_),
      functions(std::make_shared<util::Pending<ShaderFunctions>>()) {
    functions->set(ShaderFunctions{.vertex = std::move(vert), .fragment = std::move(frag), .error = {}});
}

ShaderProgram::ShaderProgram(std::string name,
                             RendererBackend& backend_,
                             PendingShaderFunctions functions_,
                             CompiledCallback onCompiled_)
    : ShaderProgramBase(),
      shaderName(std::move(name)),
      backend(backend_),
      functions(std::move(functions_)),
      onCompiled(std::move(onCompiled_)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

MTLRenderPipelineStatePtr ShaderProgram::getRenderPipelineState(const gfx::Renderable& renderable,
                                                                const MTLVertexDescriptorPtr& vertexDescriptor,
                                                                const gfx::ColorMode& colorMode,
                                                                const std::optional<std::size_t> reuseHash) const {
    const auto* awaited = awaitFunctions();
    if (!awaited) {
        return {};
    }
    const auto& compiled = *awaited;
    if (onCompiled) {
        const auto callback = std::move(onCompiled);
        onCompiled = nullptr;
        callback(compiled);
    }
    if (!compiled.vertex) {
        return {};
    }

    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

    const auto& renderableResource = renderable.getResource<RenderableResource>();

    auto colorFormat = MTL::PixelFormat::PixelFormatBGRA8Unorm;
    std::optional<MTL::PixelFormat> depthFormat = std::nullopt;
    std::optional<MTL::PixelFormat> stencilFormat = std::nullopt;
    if (const auto& rpd = renderableResource.getRenderPassDescriptor()) {
        if (auto* colorTarget = rpd->colorAttachments()->object(0)) {
            if (auto* tex = colorTarget->texture()) {
                colorFormat = tex->pixelFormat();
            }
        }
        if (auto* depthTarget = rpd->depthAttachment()) {
            if (auto* tex = depthTarget->texture()) {
                depthFormat = tex->pixelFormat();
            }
        }
        if (auto* stencilTarget = rpd->stencilAttachment()) {
            if (auto* tex = stencilTarget->texture()) {
                stencilFormat = tex->pixelFormat();
            }
        }
    }

    // A pipeline state is only valid for the attachment formats it was built with. The caller's
    // reuse hash covers the colour mode and vertex layout; fold the renderable's colour, depth
    // and stencil formats in too, or a state built for the BGRA8 + stencil screen gets reused for
    // an RGBA8 offscreen target (terrain drape targets, hillshade prepare targets) and Metal's
    // API validation aborts on the mismatch.
    std::optional<std::size_t> cacheKey;
    if (reuseHash.has_value()) {
        cacheKey = mln::util::hash(
            reuseHash.value(),
            static_cast<std::size_t>(colorFormat),
            static_cast<std::size_t>(depthFormat.value_or(MTL::PixelFormat::PixelFormatInvalid)),
            static_cast<std::size_t>(stencilFormat.value_or(MTL::PixelFormat::PixelFormatInvalid)));
        if (auto it = renderPipelineStateCache.find(*cacheKey); it != renderPipelineStateCache.end()) {
            return it->second;
        }
        if (pendingPipelineStates.contains(*cacheKey)) {
            return awaitPipelineState(*cacheKey);
        }
    }

    auto desc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
    desc->setLabel(NS::String::string(shaderName.data(), NS::UTF8StringEncoding));
    desc->setVertexFunction(compiled.vertex.get());
    desc->setFragmentFunction(compiled.fragment.get());
    desc->setVertexDescriptor(vertexDescriptor.get());

    if (auto* colorTarget = desc->colorAttachments()->object(0)) {
        const auto blendEnabled = !colorMode.blendFunction.is<gfx::ColorMode::Replace>();
        auto blendOperation = MTL::BlendOperationAdd;
        auto srcFactor = MTL::BlendFactorOne;
        auto destFactor = MTL::BlendFactorOne;

        if (blendEnabled) {
            apply_visitor(
                [&](const auto& blendFunction) {
                    blendOperation = metalBlendOperation(gfx::ColorBlendEquationType(blendFunction.equation));
                    srcFactor = metalBlendFactor(blendFunction.srcFactor);
                    destFactor = metalBlendFactor(blendFunction.dstFactor);
                },
                colorMode.blendFunction);
        }

        colorTarget->setPixelFormat(colorFormat);
        colorTarget->setBlendingEnabled(blendEnabled);
        colorTarget->setRgbBlendOperation(blendOperation);
        colorTarget->setAlphaBlendOperation(blendOperation);
        colorTarget->setSourceRGBBlendFactor(srcFactor);
        colorTarget->setSourceAlphaBlendFactor(srcFactor);
        colorTarget->setDestinationRGBBlendFactor(destFactor);
        colorTarget->setDestinationAlphaBlendFactor(destFactor);

        colorTarget->setWriteMask((colorMode.mask.r ? MTL::ColorWriteMaskRed : MTL::ColorWriteMaskNone) |
                                  (colorMode.mask.g ? MTL::ColorWriteMaskGreen : MTL::ColorWriteMaskNone) |
                                  (colorMode.mask.b ? MTL::ColorWriteMaskBlue : MTL::ColorWriteMaskNone) |
                                  (colorMode.mask.a ? MTL::ColorWriteMaskAlpha : MTL::ColorWriteMaskNone));
    }

    if (depthFormat) {
        desc->setDepthAttachmentPixelFormat(*depthFormat);
    }

    if (stencilFormat) {
        desc->setStencilAttachmentPixelFormat(*stencilFormat);
    }

    const auto& device = backend.getDevice();
    // A state no reuse key can find again is made on the spot, as before.
    if (backend.compilesShadersAsynchronously() && cacheKey) {
        auto pending = std::make_shared<util::Pending<MTLRenderPipelineStatePtr>>();
        pendingPipelineStates[*cacheKey] = pending;
        device->newRenderPipelineState(
            desc.get(), [pending, name = shaderName](MTL::RenderPipelineState* state, NS::Error* error) {
                if (!state) {
                    const auto* why = error ? error->localizedDescription()->utf8String() : "";
                    Log::Error(Event::Shader, name + " newRenderPipelineState failed: " + why);
                }
                pending->set(NS::RetainPtr(state));
            });
        return awaitPipelineState(*cacheKey);
    }

    NS::Error* error = nullptr;
    auto rps = NS::TransferPtr(device->newRenderPipelineState(desc.get(), &error));

    if (!rps || error) {
        const auto errPtr = error ? error->localizedDescription()->utf8String() : nullptr;
        const auto errStr = (errPtr && errPtr[0]) ? ": "s + errPtr : std::string();
        Log::Error(Event::Shader, shaderName + " newRenderPipelineState failed" + errStr);
        assert(false);
    }

    if (reuseHash.has_value()) {
        // store the value for future reuse
        renderPipelineStateCache[*cacheKey] = rps;
    }

    return rps;
}

namespace {
// Long enough for a cache hit to draw in the same frame, short enough not to hold it.
constexpr std::chrono::microseconds compileWait{4000};
} // namespace

const ShaderFunctions* ShaderProgram::awaitFunctions() const {
    if (!backend.compilesShadersAsynchronously()) {
        return &functions->wait();
    }
    if (const auto* done = functions->waitFor(compileWait)) {
        return done;
    }
    backend.getContext().noteDrawWaitingOnCompile();
    return nullptr;
}

MTLRenderPipelineStatePtr ShaderProgram::awaitPipelineState(std::size_t cacheKey) const {
    const auto pending = pendingPipelineStates.at(cacheKey);
    const auto* state = pending->waitFor(compileWait);
    if (!state) {
        backend.getContext().noteDrawWaitingOnCompile();
        return {};
    }
    auto made = *state;
    pendingPipelineStates.erase(cacheKey);
    assert(made);
    if (made) {
        renderPipelineStateCache[cacheKey] = made;
    }
    return made;
}

bool ShaderProgram::isWaitingOnCompile() const {
    return !functions->isSet() || !pendingPipelineStates.empty();
}

std::optional<size_t> ShaderProgram::getSamplerLocation(const size_t id) const {
    return (id < textureBindings.size()) ? textureBindings[id] : std::nullopt;
}

void ShaderProgram::initVertexAttribute(const shaders::AttributeInfo& info) {
    const auto index = static_cast<int>(info.index);
    const auto bufferIndex = static_cast<int>(info.bufferIndex);
#if !defined(NDEBUG)
    // Indexes must be unique, if there's a conflict check the `attributes` array in the shader
    vertexAttributes.visitAttributes([&](const gfx::VertexAttribute& attrib) { assert(attrib.getIndex() != index); });
#endif
    vertexAttributes.set(info.id, index, info.dataType, bufferIndex);
}

void ShaderProgram::initInstanceAttribute(const shaders::AttributeInfo& info) {
    // Index is the block index of the instance attribute
    const auto index = static_cast<int>(info.index);
    const auto bufferIndex = static_cast<int>(info.bufferIndex);
#if !defined(NDEBUG)
    // Indexes must not be reused by regular attributes or uniform blocks
    // More than one instance attribute can have the same index, if they share the block
    instanceAttributes.visitAttributes([&](const gfx::VertexAttribute& attrib) { assert(attrib.getIndex() != index); });
#endif
    instanceAttributes.set(info.id, index, info.dataType, bufferIndex);
}

void ShaderProgram::initTexture(const shaders::TextureInfo& info) {
    assert(info.id < textureBindings.size());
    if (info.id >= textureBindings.size()) {
        return;
    }
    textureBindings[info.id] = info.index;
}

} // namespace mtl
} // namespace mln
