#include <Renderer/PostProcessEffect.h>
#include <algorithm>

namespace Pyxis {

SDL_GPUTextureFormat GetGBufferTargetFormat(GBufferTarget target) {
    switch (target) {
    case GBufferTarget::Albedo:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case GBufferTarget::NormalUV:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case GBufferTarget::PositionWS:
        return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case GBufferTarget::Emission:
        return SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
    case GBufferTarget::Occlusion:
        return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

PostProcessEffect::PostProcessEffect(const std::string &name,
                                     const std::string &fragmentShaderPath,
                                     SDL_GPUTextureFormat outputFormat)
    : m_Name(name), m_OutputFormat(outputFormat) {
    PipelineStateDescription description;
    description.name = name;
    description.vertexShaderPath = "assets/shaders/DeferredEmissionVertex.hlsl";
    description.fragmentShaderPath = fragmentShaderPath;
    description.vertexStride = sizeof(glm::vec4);
    description.vertexAttributes = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = 0}};
    // replaces its whole target (every texel must be written)
    description.colorTargets = {
        {.format = outputFormat, .blend = BlendStates::Opaque()}};
    description.depth.hasDepthTarget = false;
    m_State = std::make_unique<PipelineState>(description);
    PX_VERIFY(IsValid(), "Post-process effect {} failed to build ({})", name,
              fragmentShaderPath);
}

PostProcessEffect::PostProcessEffect(const std::string &name,
                                     const std::string &fragmentShaderPath,
                                     GBufferTarget gBufferOutput)
    : PostProcessEffect(name, fragmentShaderPath,
                        GetGBufferTargetFormat(gBufferOutput)) {
    m_GBufferOutput = gBufferOutput;
}

void PostProcessEffect::Release() { m_State = nullptr; }

EdgeNormalEffect::EdgeNormalEffect()
    : PostProcessEffect("GBufferEdgeNormal",
                        "assets/shaders/GBufferEdgeNormalFragment.hlsl",
                        GBufferTarget::NormalUV) {
    UpdateUniform();
}

void EdgeNormalEffect::UpdateUniform() {
    struct EdgeNormalSettings {
        float strength;
        float edgeWidthTexels;
        float applyToWalls;
        float padding;
    } settings = {std::clamp(m_Strength, 0.0f, 1.0f),
                  std::clamp(m_EdgeWidthTexels, 1.0f, 8.0f),
                  m_ApplyToWalls ? 1.0f : 0.0f, 0.0f};
    SetUniformData(settings);
}

} // namespace Pyxis
