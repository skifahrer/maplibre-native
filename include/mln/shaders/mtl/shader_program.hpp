#pragma once

#include <mln/shaders/shader_program_base.hpp>
#include <mln/mtl/mtl_fwd.hpp>
#include <mln/mtl/vertex_attribute.hpp>
#include <mln/util/pending.hpp>

#include <Foundation/NSSharedPtr.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace mln {
namespace shaders {

struct AttributeInfo {
    constexpr AttributeInfo(std::size_t index_,
                            gfx::AttributeDataType dataType_,
                            std::size_t bufferIndex_,
                            std::size_t id_)
        : index(index_),
          dataType(dataType_),
          bufferIndex(bufferIndex_),
          id(id_) {}
    std::size_t index;
    gfx::AttributeDataType dataType;
    std::size_t bufferIndex;
    std::size_t id;
};

struct TextureInfo {
    constexpr TextureInfo(std::size_t index_, std::size_t id_)
        : index(index_),
          id(id_) {}
    std::size_t index;
    std::size_t id;
};

} // namespace shaders
namespace mtl {
class RenderableResource;
class RendererBackend;
class ShaderProgram;
using UniqueShaderProgram = std::unique_ptr<ShaderProgram>;

/// A program's functions, or why Metal could not make them (vertex is null then).
struct ShaderFunctions {
    MTLFunctionPtr vertex;
    MTLFunctionPtr fragment;
    std::string error;
};
using PendingShaderFunctions = std::shared_ptr<util::Pending<ShaderFunctions>>;

class ShaderProgram final : public gfx::ShaderProgramBase {
public:
    using CompiledCallback = std::function<void(const ShaderFunctions&)>;

    ShaderProgram(std::string name,
                  RendererBackend& backend,
                  MTLFunctionPtr vertexFunction,
                  MTLFunctionPtr fragmentFunction);
    /// `onCompiled` runs once, on the render thread, when a draw first needs the functions.
    ShaderProgram(std::string name,
                  RendererBackend& backend,
                  PendingShaderFunctions functions,
                  CompiledCallback onCompiled);
    ~ShaderProgram() noexcept override;

    static constexpr std::string_view Name{"GenericMTLShader"};
    const std::string_view typeName() const noexcept override { return Name; }

    MTLRenderPipelineStatePtr getRenderPipelineState(const gfx::Renderable&,
                                                     const MTLVertexDescriptorPtr&,
                                                     const gfx::ColorMode& colorMode,
                                                     const std::optional<std::size_t> reuseHash) const;

    std::optional<size_t> getSamplerLocation(const size_t id) const override;

    const gfx::VertexAttributeArray& getVertexAttributes() const override { return vertexAttributes; }

    const gfx::VertexAttributeArray& getInstanceAttributes() const override { return instanceAttributes; }

    void initVertexAttribute(const shaders::AttributeInfo&);
    void initInstanceAttribute(const shaders::AttributeInfo&);
    void initTexture(const shaders::TextureInfo&);

protected:
    std::string shaderName;
    RendererBackend& backend;
    PendingShaderFunctions functions;
    mutable CompiledCallback onCompiled;
    VertexAttributeArray vertexAttributes;
    VertexAttributeArray instanceAttributes;
    std::array<std::optional<size_t>, shaders::maxTextureCountPerShader> textureBindings;

    mutable mln::unordered_map<std::size_t, MTLRenderPipelineStatePtr> renderPipelineStateCache;
};

} // namespace mtl
} // namespace mln
