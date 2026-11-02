#pragma once

#include <Renderer/Material.h>
#include <Renderer/Renderer.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

// helper functions for basic blend states
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

//////////  PIPELINES  //////////
// Pipelines are the abstraction over using a set of shaders,
// a vertex format, and output texture formats.
class Pipeline {

    struct ColorTarget {
        SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        SDL_GPUColorTargetBlendState blend = BlendStates::Opaque();
    };

    struct DepthTarget {
        // whether the pipeline renders with a depth target at all (the render
        // pass then needs a depth attachment of `format`)
        bool hasDepthTarget = false;
        SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        bool testEnabled = true;
        bool writeEnabled = true;
        SDL_GPUCompareOp compareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    };

    // Everything needed to build a Pipeline.
    struct PipelineDescription {
        std::string name = "pipeline";
        // HLSL source files for shaders
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        // vertex buffer layout (one interleaved vertex buffer, slot 0)
        uint32_t vertexSize = 0;
        std::vector<SDL_GPUVertexAttribute> vertexAttributes;
        // outputs, in SV_Target order; must match the render pass attachments
        std::vector<ColorTarget> colorTargets;
        DepthTarget depthTarget;
        SDL_GPUPrimitiveType primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    };

  protected:
    SDL_GPUDevice *m_GPUDevice = nullptr;
    SDL_GPUGraphicsPipeline *m_GraphicsPipeline;
    PipelineDescription m_Description;

  public:
    friend class RenderPass;

    // Main constructor
    Pipeline(SDL_GPUDevice *gpuDevice, const PipelineDescription &description);

    ~Pipeline();

  private:
    SDL_GPUShader *CompileShader(const std::string &shaderPath,
                                 SDL_ShaderCross_ShaderStage type);
};
} // namespace Pyxis
