#include "Core/Core.h"
#include "Renderer/DefaultRenderer.h"
#include "Renderer/Renderer.h"
#include <Renderer/DeferredRenderer.h>
#include <SDL3/SDL_gpu.h>
#include <cstddef>

namespace Pyxis {

glm::ivec2 DeferredRenderer::s_RenderResolution = {480, 270};

int DeferredRenderer::s_TexturePipelineID = 0;
int DeferredRenderer::s_LightingPipelineID = 0;

Ref<Material> DeferredRenderer::s_GBufferMaterial = nullptr;
Ref<Texture> DeferredRenderer::s_GTextureColor = nullptr;
Ref<Texture> DeferredRenderer::s_GTexturePosition = nullptr;
Ref<Texture> DeferredRenderer::s_GTextureNormalUV = nullptr;

Ref<Material> DeferredRenderer::s_LightingTextureMaterial = nullptr;
Ref<Texture> DeferredRenderer::s_LightingTexture = nullptr;

Ref<Texture> DeferredRenderer::s_DepthTexture = nullptr;

bool DeferredRenderer::Init(int maxQuads, const glm::ivec2 resolution) {
    s_RenderResolution = Renderer::GetResolution();

    PX_TRACE("Resolution: {}", resolution);
    SDL_GPUTextureCreateInfo tciDepth{};
    tciDepth.type = SDL_GPU_TEXTURETYPE_2D;
    tciDepth.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    tciDepth.width = resolution.x;
    tciDepth.height = resolution.y;
    tciDepth.layer_count_or_depth = 1;
    tciDepth.num_levels = 1;
    tciDepth.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    tciDepth.props = 0;
    s_DepthTexture = Renderer::CreateTexture(tciDepth, "drdst");

    SDL_GPUTextureCreateInfo tciColor{};
    tciColor.type = SDL_GPU_TEXTURETYPE_2D;
    tciColor.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tciColor.width = resolution.x;
    tciColor.height = resolution.y;
    tciColor.layer_count_or_depth = 1;
    tciColor.num_levels = 1;
    tciColor.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tciColor.props = 0;
    s_GTextureColor = Renderer::CreateTexture(tciColor, "DeferredGColor");

    SDL_GPUTextureCreateInfo tciNormalUV{};
    tciNormalUV.type = SDL_GPU_TEXTURETYPE_2D;
    tciNormalUV.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    tciNormalUV.width = resolution.x;
    tciNormalUV.height = resolution.y;
    tciNormalUV.layer_count_or_depth = 1;
    tciNormalUV.num_levels = 1;
    tciNormalUV.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tciNormalUV.props = 0;
    s_GTextureNormalUV =
        Renderer::CreateTexture(tciNormalUV, "DeferredGNormalUV");

    SDL_GPUTextureCreateInfo tciPosition{};
    tciPosition.type = SDL_GPU_TEXTURETYPE_2D;
    tciPosition.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    tciPosition.width = resolution.x;
    tciPosition.height = resolution.y;
    tciPosition.layer_count_or_depth = 1;
    tciPosition.num_levels = 1;
    tciPosition.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tciPosition.props = 0;
    s_GTexturePosition =
        Renderer::CreateTexture(tciPosition, "DeferredGPosition");

    // material used by lighting pipeline
    s_GBufferMaterial = CreateRef<Material>(0);
    s_GBufferMaterial->SetTexture(0, s_GTextureColor);
    s_GBufferMaterial->SetTexture(1, s_GTextureNormalUV);
    s_GBufferMaterial->SetTexture(2, s_GTexturePosition);

    CreateTexturePipeline(maxQuads);

    // lighting pipeline and textures
    SDL_GPUTextureCreateInfo tciLight{};
    tciLight.type = SDL_GPU_TEXTURETYPE_2D;
    tciLight.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    tciLight.width = resolution.x;
    tciLight.height = resolution.y;
    tciLight.layer_count_or_depth = 1;
    tciLight.num_levels = 1;
    tciLight.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tciLight.props = 0;
    s_LightingTexture = Renderer::CreateTexture(tciLight, "DeferredLight");

    // material used by default renderer to draw output
    s_LightingTextureMaterial = CreateRef<Material>(0);
    s_LightingTextureMaterial->SetTexture(0, s_LightingTexture);

    CreateLightingPipeline(maxQuads);

    return true;
}

void DeferredRenderer::CreateTexturePipeline(int maxQuads) {
    std::vector<SDL_GPUVertexAttribute> textureVertexAttributes{};
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        .location = 0,    // layout (location = 0) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, // vec4
        .offset = offsetof(
            DeferredTextureVertex,
            color) // start from the first byte from current buffer position

    });
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .offset = offsetof(DeferredTextureVertex, normal_uv)

    });
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        .location = 2,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .offset = offsetof(DeferredTextureVertex, position)

    });

    // define lists of targets and their info, as we will be rendering to a few
    // textures!
    std::vector<SDL_GPUColorTargetDescription> CTDs;
    std::vector<SDL_GPUColorTargetInfo> CTIs;

    SDL_GPUColorTargetDescription ctdColor{};
    ctdColor.blend_state.enable_blend = true;
    ctdColor.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ctdColor.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    ctdColor.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ctdColor.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ctdColor.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ctdColor.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ctdColor.format =
        SDL_GPUTextureFormat::SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    CTDs.push_back(ctdColor);

    SDL_GPUColorTargetInfo ctiColor{};
    // discard previous content and clear to a color
    ctiColor.clear_color = {0, 0, 0, 1};
    ctiColor.load_op = SDL_GPU_LOADOP_CLEAR;
    ctiColor.store_op = SDL_GPU_STOREOP_STORE;
    ctiColor.texture = s_GTextureColor->GetGPUTexture();
    CTIs.push_back(ctiColor);

    SDL_GPUColorTargetDescription ctdNormalUV{};
    ctdNormalUV.blend_state.enable_blend = false;
    ctdNormalUV.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    CTDs.push_back(ctdNormalUV);

    SDL_GPUColorTargetInfo ctiNormalUV{};
    // discard previous content and clear to a color
    ctiNormalUV.clear_color = {0.5, 0.5, 0, 0};
    ctiNormalUV.load_op = SDL_GPU_LOADOP_CLEAR;
    ctiNormalUV.store_op = SDL_GPU_STOREOP_STORE;
    ctiNormalUV.texture = s_GTextureNormalUV->GetGPUTexture();
    CTIs.push_back(ctiNormalUV);

    SDL_GPUColorTargetDescription ctdPosition{};
    ctdPosition.blend_state.enable_blend = false;
    ctdPosition.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    CTDs.push_back(ctdPosition);

    SDL_GPUColorTargetInfo ctiPosition{};
    // discard previous content and clear to a color
    ctiPosition.clear_color = {0, 0, 0, 1};
    ctiPosition.load_op = SDL_GPU_LOADOP_CLEAR;
    ctiPosition.store_op = SDL_GPU_STOREOP_STORE;
    ctiPosition.texture = s_GTexturePosition->GetGPUTexture();
    CTIs.push_back(ctiPosition);

    SDL_GPUDepthStencilTargetInfo dsti{};
    dsti.texture = s_DepthTexture->GetGPUTexture();
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_depth = 1.0f; // far plane
    dsti.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.stencil_store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_stencil = 0;

    s_TexturePipelineID = Renderer::CreatePipeline(
        4 * maxQuads, sizeof(DeferredTextureVertex), 6 * maxQuads,
        textureVertexAttributes, CTDs, CTIs, &dsti,
        "assets/shaders/DeferredGVertex.hlsl",
        "assets/shaders/DeferredGFragment.hlsl", false);
    if (s_TexturePipelineID < 0) {
        PX_ERROR("Failed to init DeferredRenderer texture pipeline");
    }
}

void DeferredRenderer::CreateLightingPipeline(int maxQuads) {
    std::vector<SDL_GPUVertexAttribute> lightVertexAttributes;

    lightVertexAttributes.push_back(
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredLightVertex, color)});

    lightVertexAttributes.push_back(
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredLightVertex, position)});

    lightVertexAttributes.push_back(
        {.location = 2,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredLightVertex, positionCenter)});

    lightVertexAttributes.push_back(
        {.location = 3,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredLightVertex, rad_intensity_falloff_type)});

    std::vector<SDL_GPUColorTargetDescription> LightColorTargetDescriptions;

    SDL_GPUColorTargetDescription ctdLight{};
    ctdLight.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // or R11G11B10

    ctdLight.blend_state.enable_blend = true;
    ctdLight.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ctdLight.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    ctdLight.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ctdLight.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE; // additive
    ctdLight.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ctdLight.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

    LightColorTargetDescriptions.push_back(ctdLight);

    SDL_GPUColorTargetInfo colorTargetInfo{};
    // discard previous content and clear to a color
    colorTargetInfo.clear_color = {0, 0, 0, 1};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
    colorTargetInfo.texture = s_LightingTexture->GetGPUTexture();

    std::vector<SDL_GPUColorTargetInfo> targetInfoVec;
    targetInfoVec.push_back(colorTargetInfo);

    glm::mat4 transform;
    s_LightingPipelineID = Renderer::CreatePipeline(
        4 * maxQuads, sizeof(DeferredLightVertex), 6 * maxQuads,
        lightVertexAttributes, LightColorTargetDescriptions, targetInfoVec,
        nullptr, "assets/shaders/DeferredLightVertex.hlsl",
        "assets/shaders/DeferredLightFragment.hlsl", false);
    if (s_LightingPipelineID < 0) {
        PX_ERROR("Failed to init DeferredRenderer lighting pipeline");
    }
}

void DeferredRenderer::Shutdown() {
    s_TexturePipelineID = -1;
    s_LightingPipelineID = -1;

    s_GBufferMaterial = nullptr;
    s_GTextureColor = nullptr;
    s_GTexturePosition = nullptr;
    s_GTextureNormalUV = nullptr;

    s_DepthTexture = nullptr;

    s_LightingTexture = nullptr;
    s_LightingTextureMaterial = nullptr;

    PX_TRACE("Deferred Renderer Shut Down");
}

void DeferredRenderer::OnWindowResize(const glm::ivec2 &resolution) {
    DeferredRenderer::Resize(resolution);
}

void DeferredRenderer::Resize(const glm::ivec2 &resolution) {

    Renderer::GetPipeline(s_TexturePipelineID)->SetResolution(resolution);
    Renderer::GetPipeline(s_LightingPipelineID)->SetResolution(resolution);
    s_RenderResolution = resolution;
    // When we resize the texture, the underlying sdl gpu texture is replaced.
    // This means that the pointer held by the pipeline breaks.
    // This is why I call UpdateColorTargetTexture &
    // UpdateDepthStencilTargetTexture.
    s_DepthTexture->Resize(resolution);
    Renderer::GetPipeline(s_TexturePipelineID)
        ->UpdateDepthStencilTargetTexture(s_DepthTexture);

    s_GTextureColor->Resize(resolution);
    Renderer::GetPipeline(s_TexturePipelineID)
        ->UpdateColorTargetTexture(0, s_GTextureColor);
    s_GTextureNormalUV->Resize(resolution);
    Renderer::GetPipeline(s_TexturePipelineID)
        ->UpdateColorTargetTexture(1, s_GTextureNormalUV);
    s_GTexturePosition->Resize(resolution);
    Renderer::GetPipeline(s_TexturePipelineID)
        ->UpdateColorTargetTexture(2, s_GTexturePosition);

    s_LightingTexture->Resize(resolution);
    Renderer::GetPipeline(s_LightingPipelineID)
        ->UpdateColorTargetTexture(0, s_LightingTexture);
}

void DeferredRenderer::DrawObjects() {
    Renderer::DrawPipeline(DeferredRenderer::s_TexturePipelineID);
}
void DeferredRenderer::DrawLights() {
    Renderer::DrawPipeline(s_LightingPipelineID);
}
void DeferredRenderer::DrawToScreen(float depth) {
    DefaultRenderer::DrawQuad({0, 0, depth}, {2, 2}, s_LightingTextureMaterial);
}

void DeferredRenderer::Debug_DrawColorToScreen() {
    DefaultRenderer::DrawQuad({0, 0, 0.5f}, {2, 2}, s_GBufferMaterial);
}
// TODO
void DeferredRenderer::Debug_DrawNormalUVToScreen() {}
void DeferredRenderer::Debug_DrawPositionToScreen() {}

void DeferredRenderer::DrawQuad(glm::vec3 position, glm::vec2 size,
                                Ref<Material> material, const glm::vec4 &tint,
                                const glm::vec4 &uvBounds) {
    std::vector<DeferredTextureVertex> vertices;

    vertices.push_back( // tl
        {tint,
         {0, 0, uvBounds.x, uvBounds.y},
         (glm::vec4(-0.5f, 0.5f, 0, 1) * glm::vec4(size.x, size.y, 1, 1)) +
             glm::vec4(position, 0)});
    vertices.push_back( // tr
        {tint,
         {0, 0, uvBounds.z, uvBounds.y},
         (glm::vec4(0.5f, 0.5f, 0, 1) * glm::vec4(size.x, size.y, 1, 1)) +
             glm::vec4(position, 0)});
    vertices.push_back( // bl
        {tint,
         {0, 0, uvBounds.x, uvBounds.w},
         (glm::vec4(-0.5f, -0.5f, 0, 1) * glm::vec4(size.x, size.y, 1, 1)) +
             glm::vec4(position, 0)});
    vertices.push_back( // br
        {tint,
         {0, 0, uvBounds.z, uvBounds.w},
         (glm::vec4(0.5f, -0.5f, 0, 1) * glm::vec4(size.x, size.y, 1, 1)) +
             glm::vec4(position, 0)});
    if (material == nullptr)
        Renderer::DrawToPipeline(DeferredRenderer::s_TexturePipelineID,
                                 vertices, QuadIndices,
                                 DefaultRenderer::s_DefaultMaterial);
    else
        Renderer::DrawToPipeline(s_TexturePipelineID, vertices, QuadIndices,
                                 material);
}

void DeferredRenderer::DrawText(int fontID, glm::vec3 position,
                                const std::string &text, const glm::vec4 &color,
                                const glm::vec2 scale) {
    Ref<Material> fontMaterial = Text::GetFontMaterial(fontID);
    if (fontMaterial == nullptr)
        return;

    auto commands = Text::DrawText(fontID, position, text, color, scale);
    for (auto &c : commands) {
        DrawQuad(glm::vec3(c.position, position.z), c.size, fontMaterial, color,
                 c.uvBounds);
    }
}

void DeferredRenderer::DrawLight(const glm::vec3 &position,
                                 const glm::vec4 &color, float radius,
                                 float intensity, float falloff, float type) {
    std::vector<DeferredLightVertex> vertices;

    vertices.push_back( // tl
        {color,
         (glm::vec4(-0.5f, 0.5f, 0, 1) *
          glm::vec4(radius * 2, radius * 2, 1, 1)) +
             glm::vec4(position, 0),
         glm::vec4(position, 1), glm::vec4(radius, intensity, falloff, type)});

    vertices.push_back( // tr
        {color,
         (glm::vec4(0.5f, 0.5f, 0, 1) *
          glm::vec4(radius * 2, radius * 2, 1, 1)) +
             glm::vec4(position, 0),
         glm::vec4(position, 1), glm::vec4(radius, intensity, falloff, type)});
    vertices.push_back( // bl
        {color,
         (glm::vec4(-0.5f, -0.5f, 0, 1) *
          glm::vec4(radius * 2, radius * 2, 1, 1)) +
             glm::vec4(position, 0),
         glm::vec4(position, 1), glm::vec4(radius, intensity, falloff, type)});
    vertices.push_back( // br
        {color,
         (glm::vec4(0.5f, -0.5f, 0, 1) *
          glm::vec4(radius * 2, radius * 2, 1, 1)) +
             glm::vec4(position, 0),
         glm::vec4(position, 1), glm::vec4(radius, intensity, falloff, type)});

    Renderer::DrawToPipeline(DeferredRenderer::s_LightingPipelineID, vertices,
                             QuadIndices, s_GBufferMaterial);
}

} // namespace Pyxis
