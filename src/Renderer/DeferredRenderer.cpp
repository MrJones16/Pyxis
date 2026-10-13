#include "Core/Core.h"
#include "Renderer/DefaultRenderer.h"
#include "Renderer/Renderer.h"
#include <Core/Camera.h>
#include <Renderer/DeferredRenderer.h>
#include <SDL3/SDL_gpu.h>
#include <cstddef>

namespace Pyxis {

glm::ivec2 DeferredRenderer::s_RenderResolution = {480, 270};
glm::vec4 DeferredRenderer::s_ClearColor = {0, 0, 0, 1};

Pipeline *DeferredRenderer::s_TexturePipeline = nullptr;
Pipeline *DeferredRenderer::s_LightingPipeline = nullptr;
Pipeline *DeferredRenderer::s_LinePipeline = nullptr;

Ref<Uniform> DeferredRenderer::s_CameraUniform = nullptr;

Ref<Material> DeferredRenderer::s_GBufferMaterial = nullptr;
Ref<Texture> DeferredRenderer::s_GTextureColor = nullptr;
Ref<Texture> DeferredRenderer::s_GTexturePositionNS = nullptr;
Ref<Texture> DeferredRenderer::s_GTextureNormalUV = nullptr;

Ref<Material> DeferredRenderer::s_LightingTextureMaterial = nullptr;
Ref<Texture> DeferredRenderer::s_LightingTexture = nullptr;

Ref<Texture> DeferredRenderer::s_DepthTexture = nullptr;

bool DeferredRenderer::Init(int maxQuads, const glm::ivec2 resolution) {

    PX_BEGINSTEPS("Initializing Deferred Renderer...");
    s_RenderResolution = resolution;

    SDL_GPUTextureCreateInfo tciDepth{};
    tciDepth.type = SDL_GPU_TEXTURETYPE_2D;
    tciDepth.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    tciDepth.width = resolution.x;
    tciDepth.height = resolution.y;
    tciDepth.layer_count_or_depth = 1;
    tciDepth.num_levels = 1;
    tciDepth.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    tciDepth.props = 0;
    s_DepthTexture = Texture::CreateTexture(tciDepth, "drdst");

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
    s_GTextureColor = Texture::CreateTexture(tciColor, "DeferredGColor");

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
        Texture::CreateTexture(tciNormalUV, "DeferredGNormalUV");

    SDL_GPUTextureCreateInfo tciPositionNS{};
    tciPositionNS.type = SDL_GPU_TEXTURETYPE_2D;
    tciPositionNS.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    tciPositionNS.width = resolution.x;
    tciPositionNS.height = resolution.y;
    tciPositionNS.layer_count_or_depth = 1;
    tciPositionNS.num_levels = 1;
    tciPositionNS.usage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tciPositionNS.props = 0;
    s_GTexturePositionNS =
        Texture::CreateTexture(tciPositionNS, "DeferredGPositionNS");

    // material used by lighting pipeline
    s_GBufferMaterial = CreateRef<Material>();
    s_GBufferMaterial->SetTexture(0, s_GTextureColor);
    s_GBufferMaterial->SetTexture(1, s_GTextureNormalUV);
    s_GBufferMaterial->SetTexture(2, s_GTexturePositionNS);

    // create camera uniform which will be shared by both pipelines
    s_CameraUniform = CreateRef<Uniform>();

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
    s_LightingTexture = Texture::CreateTexture(tciLight, "DeferredLight");

    // material used by default renderer to draw output
    s_LightingTextureMaterial = CreateRef<Material>();
    s_LightingTextureMaterial->SetTexture(0, s_LightingTexture);

    CreateLightingPipeline(maxQuads);
    CreateLinePipeline(maxQuads);

    PX_ENDSTEPS();
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
        .offset = offsetof(DeferredTextureVertex, position_ns)

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
    ctiColor.clear_color = {s_ClearColor.r, s_ClearColor.g, s_ClearColor.b,
                            s_ClearColor.a};
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

    SDL_GPUColorTargetDescription ctdPositionNS{};
    ctdPositionNS.blend_state.enable_blend = false;
    ctdPositionNS.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    CTDs.push_back(ctdPositionNS);

    SDL_GPUColorTargetInfo ctiPositionNS{};
    // discard previous content and clear to a color
    ctiPositionNS.clear_color = {0, 0, 0, 1};
    ctiPositionNS.load_op = SDL_GPU_LOADOP_CLEAR;
    ctiPositionNS.store_op = SDL_GPU_STOREOP_STORE;
    ctiPositionNS.texture = s_GTexturePositionNS->GetGPUTexture();
    CTIs.push_back(ctiPositionNS);

    SDL_GPUDepthStencilTargetInfo dsti{};
    dsti.texture = s_DepthTexture->GetGPUTexture();
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_depth = 1.0f; // far plane
    dsti.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.stencil_store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_stencil = 0;

    s_TexturePipeline = new Pipeline(
        s_RenderResolution, 4 * maxQuads, sizeof(DeferredTextureVertex),
        6 * maxQuads, textureVertexAttributes, CTDs, CTIs, &dsti,
        "assets/shaders/DeferredGVertex.hlsl",
        "assets/shaders/DeferredGFragment.hlsl", false);
    s_TexturePipeline->SetVertexUniform(s_CameraUniform);
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
    s_LightingPipeline = new Pipeline(
        s_RenderResolution, 4 * maxQuads, sizeof(DeferredLightVertex),
        6 * maxQuads, lightVertexAttributes, LightColorTargetDescriptions,
        targetInfoVec, nullptr, "assets/shaders/DeferredLightVertex.hlsl",
        "assets/shaders/DeferredLightFragment.hlsl", false);
    s_LightingPipeline->SetVertexUniform(s_CameraUniform);
}

void DeferredRenderer::SetClearColor(const glm::vec4 &color) {
    s_ClearColor = color;
    // the albedo target is the first color target of the G-buffer pipeline
    if (s_TexturePipeline && !s_TexturePipeline->GetColorTargets().empty())
        s_TexturePipeline->GetColorTargets()[0].clear_color = {
            color.r, color.g, color.b, color.a};
}

glm::vec4 DeferredRenderer::GetClearColor() { return s_ClearColor; }

void DeferredRenderer::Shutdown() {
    s_CameraUniform = nullptr;

    delete s_TexturePipeline;
    delete s_LightingPipeline;
    delete s_LinePipeline;
    s_LinePipeline = nullptr;

    s_GBufferMaterial = nullptr;
    s_GTextureColor = nullptr;
    s_GTexturePositionNS = nullptr;
    s_GTextureNormalUV = nullptr;

    s_DepthTexture = nullptr;

    s_LightingTexture = nullptr;
    s_LightingTextureMaterial = nullptr;

    PX_LOG("Deferred Renderer Shut Down");
}

void DeferredRenderer::OnWindowResize(const glm::ivec2 &resolution) {
    DeferredRenderer::Resize(resolution);
}

void DeferredRenderer::Resize(const glm::ivec2 &resolution) {
    s_RenderResolution = resolution;
    s_TexturePipeline->SetResolution(resolution);
    s_LightingPipeline->SetResolution(resolution);
    // When we resize the texture, the underlying sdl gpu texture is replaced.
    // This means that the pointer held by the pipeline breaks.
    // This is why I call UpdateColorTargetTexture &
    // UpdateDepthStencilTargetTexture.
    s_DepthTexture->Resize(resolution);
    s_TexturePipeline->UpdateDepthStencilTargetTexture(s_DepthTexture);

    s_GTextureColor->Resize(resolution);

    s_TexturePipeline->UpdateColorTargetTexture(0, s_GTextureColor);
    s_GTextureNormalUV->Resize(resolution);
    s_TexturePipeline->UpdateColorTargetTexture(1, s_GTextureNormalUV);
    s_GTexturePositionNS->Resize(resolution);
    s_TexturePipeline->UpdateColorTargetTexture(2, s_GTexturePositionNS);

    s_LightingTexture->Resize(resolution);
    s_LightingPipeline->UpdateColorTargetTexture(0, s_LightingTexture);
    s_LinePipeline->SetResolution(resolution);
    s_LinePipeline->UpdateColorTargetTexture(0, s_LightingTexture);
}

void DeferredRenderer::SetViewProjectionMatrix(
    glm::mat4 &ViewProjectionMatrix) {
    s_CameraUniform->UpdateData(sizeof(glm::mat4), &ViewProjectionMatrix);
}

void DeferredRenderer::DrawObjects(Renderer::FrameData &frameData) {
    s_TexturePipeline->Draw(frameData);
}
void DeferredRenderer::DrawLights(Renderer::FrameData &frameData) {
    s_LightingPipeline->Draw(frameData);
    // unlit debug lines on top of the lit image
    s_LinePipeline->Draw(frameData);
}

void DeferredRenderer::CreateLinePipeline(int maxLines) {
    std::vector<SDL_GPUVertexAttribute> attributes;
    attributes.push_back(SDL_GPUVertexAttribute{
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .offset = offsetof(DeferredLineVertex, color)});
    attributes.push_back(SDL_GPUVertexAttribute{
        .location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .offset = offsetof(DeferredLineVertex, position)});

    // same format as the lighting texture, alpha blended over it
    SDL_GPUColorTargetDescription ctd{};
    ctd.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    ctd.blend_state.enable_blend = true;
    ctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    ctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ctd.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ctd.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    // keep the lit image, draw on top of it
    SDL_GPUColorTargetInfo cti{};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = s_LightingTexture->GetGPUTexture();

    s_LinePipeline = new Pipeline(
        s_RenderResolution, 2 * maxLines, sizeof(DeferredLineVertex),
        2 * maxLines, attributes, {ctd}, {cti}, nullptr,
        "assets/shaders/DeferredLineVertex.hlsl",
        "assets/shaders/DeferredLineFragment.hlsl", false,
        SDL_GPU_PRIMITIVETYPE_LINELIST);
    s_LinePipeline->SetVertexUniform(s_CameraUniform);
}

static const std::vector<uint32_t> s_LineIndices = {0, 1};

void DeferredRenderer::DrawLine(const glm::vec3 &start, const glm::vec3 &end,
                                const glm::vec4 &color) {
    std::vector<DeferredLineVertex> vertices = {
        {color, glm::vec4(start, 1)}, {color, glm::vec4(end, 1)}};
    s_LinePipeline->QueueMesh(vertices, s_LineIndices, nullptr);
}

void DeferredRenderer::DrawLine(const glm::vec2 &start, const glm::vec2 &end,
                                const glm::vec4 &color, float z) {
    DrawLine(glm::vec3(start, z), glm::vec3(end, z), color);
}

void DeferredRenderer::DrawLines(const std::vector<glm::vec2> &points,
                                 const glm::vec4 &color, bool closed,
                                 float z) {
    if (points.size() < 2)
        return;
    std::vector<DeferredLineVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(points.size());
    for (auto &p : points)
        vertices.push_back({color, glm::vec4(p, z, 1)});
    const uint32_t n = (uint32_t)points.size();
    for (uint32_t i = 0; i + 1 < n; i++) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    if (closed && n > 2) {
        indices.push_back(n - 1);
        indices.push_back(0);
    }
    s_LinePipeline->QueueMesh(vertices, indices, nullptr);
}
void DeferredRenderer::DrawToScreen(float depth, bool useMainCamera) {
    if (useMainCamera) {
        glm::vec3 cameraOffset = Camera::GetOffset();
        glm::vec2 uvOffset =
            (glm::vec2)cameraOffset / (glm::vec2)Camera::GetSize();
        DefaultRenderer::DrawQuad({-uvOffset.x * 2, -uvOffset.y * 2, depth},
                                  {2, 2}, s_LightingTextureMaterial);
    } else {
        DefaultRenderer::DrawQuad({0, 0, depth}, {2, 2},
                                  s_LightingTextureMaterial);
    }
}

void DeferredRenderer::Debug_DrawColorToScreen() {
    DefaultRenderer::DrawQuad({0, 0, 0.5f}, {2, 2}, s_GBufferMaterial);
}

// TODO, havent needed yet
void DeferredRenderer::Debug_DrawNormalUVToScreen() {}
void DeferredRenderer::Debug_DrawPositionToScreen() {}

void DeferredRenderer::DrawQuad(glm::vec3 position, glm::vec2 size,
                                Ref<Bindable> bindable, const glm::vec4 &tint,
                                const glm::vec4 &uvBounds,
                                const float normalStrength) {
    std::vector<DeferredTextureVertex> vertices;

    vertices.push_back( // tl
        {tint,
         {-0.5, 0.5, uvBounds.x, uvBounds.y},
         (glm::vec4(-0.5f, 0.5f, 0, 0) * glm::vec4(size.x, size.y, 0, 0)) +
             glm::vec4(position, normalStrength)});
    vertices.push_back( // tr
        {tint,
         {0.5, 0.5, uvBounds.z, uvBounds.y},
         (glm::vec4(0.5f, 0.5f, 0, 0) * glm::vec4(size.x, size.y, 0, 0)) +
             glm::vec4(position, normalStrength)});
    vertices.push_back( // bl
        {tint,
         {-0.5, -0.5, uvBounds.x, uvBounds.w},
         (glm::vec4(-0.5f, -0.5f, 0, 0) * glm::vec4(size.x, size.y, 0, 0)) +
             glm::vec4(position, normalStrength)});
    vertices.push_back( // br
        {tint,
         {0.5, -0.5, uvBounds.z, uvBounds.w},
         (glm::vec4(0.5f, -0.5f, 0, 0) * glm::vec4(size.x, size.y, 0, 0)) +
             glm::vec4(position, normalStrength)});
    if (bindable == nullptr)
        s_TexturePipeline->QueueMesh(vertices, QuadIndices,
                                     DefaultRenderer::s_WhiteTexture);
    else
        s_TexturePipeline->QueueMesh(vertices, QuadIndices, bindable);
}

void DeferredRenderer::DrawText(Ref<Font> font, glm::vec3 position,
                                const std::string &text, const glm::vec4 &color,
                                const glm::vec2 scale) {

    auto commands = Text::DrawText(font, position, text, color, scale);
    for (auto &c : commands) {
        DrawQuad(glm::vec3(c.position, position.z), c.size, font->GetTexture(),
                 color, c.uvBounds, 0);
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
    s_LightingPipeline->QueueMesh(vertices, QuadIndices, s_GBufferMaterial);
}

} // namespace Pyxis
