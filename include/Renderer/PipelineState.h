#pragma once
#include <Renderer/Renderer.h>
#include <string>
#include <vector>

namespace Pyxis {

// Ready-made blend states for PipelineStateDescription color targets
namespace BlendStates {
// no blending: the shader output replaces the target
inline SDL_GPUColorTargetBlendState Opaque() { return {}; }
// classic alpha blending: src * srcAlpha + dst * (1 - srcAlpha)
inline SDL_GPUColorTargetBlendState Alpha() {
    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = true;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    return blend;
}
// additive: src + dst (e.g. light accumulation)
inline SDL_GPUColorTargetBlendState Additive() {
    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = true;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    return blend;
}
} // namespace BlendStates

// One color output of a pipeline: the format it writes and how it blends.
struct PipelineColorTarget {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUColorTargetBlendState blend = BlendStates::Opaque();
};

// Depth / stencil usage of a pipeline.
struct PipelineDepthState {
    // whether the pipeline renders with a depth target at all (the render
    // pass then needs a depth attachment of `format`)
    bool hasDepthTarget = false;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    bool testEnabled = true;
    bool writeEnabled = true;
    SDL_GPUCompareOp compareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
};

// Everything needed to build a PipelineState.
struct PipelineStateDescription {
    std::string name = "pipeline";
    // HLSL source files, compiled at runtime (SDL_shadercross, entry "main")
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    // vertex buffer layout (one interleaved vertex buffer, slot 0)
    uint32_t vertexStride = 0;
    std::vector<SDL_GPUVertexAttribute> vertexAttributes;
    // outputs, in SV_Target order; must match the render pass attachments
    std::vector<PipelineColorTarget> colorTargets;
    PipelineDepthState depth;
    SDL_GPUPrimitiveType primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
};

// A compiled shader pair plus fixed-function state (vertex layout, blend,
// depth, primitive type): an SDL_GPUGraphicsPipeline. It holds no geometry
// and no render targets; draw it with Renderer::Draw(state, batch) inside a
// render pass whose attachments match its color targets / depth state.
class PipelineState {
  public:
    explicit PipelineState(const PipelineStateDescription &description);
    ~PipelineState();
    PipelineState(const PipelineState &) = delete;
    PipelineState &operator=(const PipelineState &) = delete;

    // false if a shader failed to load/compile or SDL refused the pipeline
    // (logged); drawing an invalid state does nothing
    inline bool IsValid() const { return m_GraphicsPipeline != nullptr; }
    inline const PipelineStateDescription &GetDescription() const {
        return m_Description;
    }
    inline SDL_GPUGraphicsPipeline *GetGPUPipeline() const {
        return m_GraphicsPipeline;
    }

    // Releases the GPU pipeline now (before the device is destroyed);
    // the state becomes invalid.
    void Release();

  private:
    PipelineStateDescription m_Description;
    SDL_GPUGraphicsPipeline *m_GraphicsPipeline = nullptr;
};

} // namespace Pyxis
