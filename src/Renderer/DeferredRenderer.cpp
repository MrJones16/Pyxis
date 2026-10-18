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

Ref<DeferredGPassShader> DeferredRenderer::s_DefaultGPassShader = nullptr;
std::vector<std::weak_ptr<DeferredGPassShader>>
    DeferredRenderer::s_GPassShaders = {};
int DeferredRenderer::s_DefaultMaxQuads = 10000;

std::unique_ptr<PipelineState> DeferredRenderer::s_LightingState = nullptr;
std::unique_ptr<BatchBuffer> DeferredRenderer::s_LightingBatch = nullptr;
std::unique_ptr<PipelineState> DeferredRenderer::s_LineState = nullptr;
std::unique_ptr<BatchBuffer> DeferredRenderer::s_LineBatch = nullptr;

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

    // G-buffer textures, read by the lighting shader
    s_GBufferMaterial = CreateRef<Material>();
    s_GBufferMaterial->SetTexture(0, s_GTextureColor);
    s_GBufferMaterial->SetTexture(1, s_GTextureNormalUV);
    s_GBufferMaterial->SetTexture(2, s_GTexturePositionNS);

    // camera uniform shared by every G-pass shader, lights and lines
    s_CameraUniform = CreateRef<Uniform>();

    s_DefaultMaxQuads = maxQuads;
    DeferredGPassShaderOptions defaultOptions;
    defaultOptions.maxQuads = maxQuads;
    s_DefaultGPassShader = CreateGPassShader(
        "DeferredGDefault", "assets/shaders/DeferredGVertex.hlsl",
        "assets/shaders/DeferredGFragment.hlsl", defaultOptions);

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

    CreateLightingState(maxQuads);
    CreateLineState(maxQuads);

    PX_ENDSTEPS();
    return true;
}

////////////////////////
/// G-PASS SHADERS ///
////////////////////////

DeferredGPassShader::DeferredGPassShader(const std::string &name,
                                         std::unique_ptr<PipelineState> state,
                                         std::unique_ptr<BatchBuffer> batch)
    : m_Name(name), m_State(std::move(state)), m_Batch(std::move(batch)) {}

void DeferredGPassShader::Release() {
    m_Batch = nullptr;
    m_State = nullptr;
}

Ref<DeferredGPassShader> DeferredRenderer::CreateGPassShader(
    const std::string &name, const std::string &vertexShaderPath,
    const std::string &fragmentShaderPath,
    const DeferredGPassShaderOptions &options) {
    PipelineStateDescription description;
    description.name = name;
    description.vertexShaderPath = vertexShaderPath;
    description.fragmentShaderPath = fragmentShaderPath;
    description.vertexStride = sizeof(DeferredTextureVertex);
    description.vertexAttributes = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredTextureVertex, color)},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredTextureVertex, normal_uv)},
        {.location = 2,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(DeferredTextureVertex, position_ns)},
    };
    // the G-buffer: albedo (blended), normal/uv and position/normal
    // strength (never blended)
    description.colorTargets = {
        {.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
         .blend = options.albedoBlend},
        {.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
         .blend = BlendStates::Opaque()},
        {.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
         .blend = BlendStates::Opaque()},
    };
    description.depth.hasDepthTarget = true;
    description.depth.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    description.depth.testEnabled = options.depthTestEnabled;
    description.depth.writeEnabled = options.depthWriteEnabled;
    description.depth.compareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    auto state = std::make_unique<PipelineState>(description);
    // submission order: the depth buffer orders opaque pixels, and equal
    // depths keep the order they were drawn in
    auto batch = std::make_unique<BatchBuffer>(
        name, (uint32_t)sizeof(DeferredTextureVertex), 4 * options.maxQuads,
        6 * options.maxQuads, BatchBuffer::DrawOrder::Submission);
    Ref<DeferredGPassShader> shader = CreateRef<DeferredGPassShader>(
        name, std::move(state), std::move(batch));
    PX_VERIFY(shader->IsValid(), "G-pass shader {} failed to build ({} / {})",
              name, vertexShaderPath, fragmentShaderPath);
    s_GPassShaders.push_back(shader);
    return shader;
}

Ref<DeferredGPassShader> DeferredRenderer::GetDefaultGPassShader() {
    return s_DefaultGPassShader;
}

void DeferredRenderer::CreateLightingState(int maxQuads) {
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

    PipelineStateDescription description;
    description.name = "DeferredLight";
    description.vertexShaderPath = "assets/shaders/DeferredLightVertex.hlsl";
    description.fragmentShaderPath =
        "assets/shaders/DeferredLightFragment.hlsl";
    description.vertexStride = sizeof(DeferredLightVertex);
    description.vertexAttributes = lightVertexAttributes;
    // lights add up in the lighting texture
    description.colorTargets = {{.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                                 .blend = BlendStates::Additive()}};
    description.depth.hasDepthTarget = false;
    s_LightingState = std::make_unique<PipelineState>(description);
    s_LightingBatch = std::make_unique<BatchBuffer>(
        "DeferredLight", (uint32_t)sizeof(DeferredLightVertex), 4 * maxQuads,
        6 * maxQuads, BatchBuffer::DrawOrder::Submission);
}

void DeferredRenderer::SetClearColor(const glm::vec4 &color) {
    // used as the albedo clear color by the next DrawObjects
    s_ClearColor = color;
}

glm::vec4 DeferredRenderer::GetClearColor() { return s_ClearColor; }

void DeferredRenderer::Shutdown() {
    s_CameraUniform = nullptr;

    // free the GPU objects of every G-pass shader still referenced (the
    // handles themselves stay valid but become invalid / skipped)
    for (auto &weakShader : s_GPassShaders)
        if (Ref<DeferredGPassShader> shader = weakShader.lock())
            shader->Release();
    s_GPassShaders.clear();
    s_DefaultGPassShader = nullptr;

    s_LightingBatch = nullptr;
    s_LightingState = nullptr;
    s_LineBatch = nullptr;
    s_LineState = nullptr;

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
    // render passes look the textures up when they are recorded, so resizing
    // them (which replaces the SDL textures) is all that's needed
    s_DepthTexture->Resize(resolution);
    s_GTextureColor->Resize(resolution);
    s_GTextureNormalUV->Resize(resolution);
    s_GTexturePositionNS->Resize(resolution);
    s_LightingTexture->Resize(resolution);
}

void DeferredRenderer::SetViewProjectionMatrix(
    glm::mat4 &ViewProjectionMatrix) {
    s_CameraUniform->UpdateData(sizeof(glm::mat4), &ViewProjectionMatrix);
}

void DeferredRenderer::DrawObjects() {
    RenderPassDescription pass;
    pass.name = "DeferredGPass";
    RenderPassColorAttachment color;
    color.texture = s_GTextureColor;
    color.clearColor = s_ClearColor;
    RenderPassColorAttachment normalUV;
    normalUV.texture = s_GTextureNormalUV;
    normalUV.clearColor = {0.5f, 0.5f, 0, 0};
    RenderPassColorAttachment positionNS;
    positionNS.texture = s_GTexturePositionNS;
    positionNS.clearColor = {0, 0, 0, 1};
    pass.colorAttachments = {color, normalUV, positionNS};
    pass.hasDepthAttachment = true;
    pass.depthAttachment.texture = s_DepthTexture;
    pass.depthAttachment.clearDepth = 1.0f; // far plane

    // drop the shaders nobody references anymore
    std::erase_if(s_GPassShaders,
                  [](const std::weak_ptr<DeferredGPassShader> &shader) {
                      return shader.expired();
                  });

    if (Renderer::BeginPass(pass)) {
        for (auto &weakShader : s_GPassShaders) {
            Ref<DeferredGPassShader> shader = weakShader.lock();
            if (shader && shader->IsValid() && !shader->GetBatch().IsEmpty())
                Renderer::Draw(shader->GetPipelineState(), shader->GetBatch(),
                               s_CameraUniform.get());
        }
        Renderer::EndPass();
    }
    for (auto &weakShader : s_GPassShaders)
        if (Ref<DeferredGPassShader> shader = weakShader.lock();
            shader && shader->IsValid())
            shader->GetBatch().Clear();
}

void DeferredRenderer::DrawLights() {
    RenderPassDescription pass;
    pass.name = "DeferredLighting";
    RenderPassColorAttachment lighting;
    lighting.texture = s_LightingTexture;
    lighting.clearColor = {0, 0, 0, 1};
    pass.colorAttachments = {lighting};
    if (Renderer::BeginPass(pass)) {
        Renderer::Draw(*s_LightingState, *s_LightingBatch,
                       s_CameraUniform.get());
        // unlit debug lines on top of the lit image
        Renderer::Draw(*s_LineState, *s_LineBatch, s_CameraUniform.get());
        Renderer::EndPass();
    }
    s_LightingBatch->Clear();
    s_LineBatch->Clear();
}

void DeferredRenderer::CreateLineState(int maxLines) {
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

    // same format as the lighting texture, alpha blended over it (alpha
    // accumulates: src ONE, dst ONE_MINUS_SRC_ALPHA)
    SDL_GPUColorTargetBlendState blend = BlendStates::Alpha();
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

    PipelineStateDescription description;
    description.name = "DeferredLine";
    description.vertexShaderPath = "assets/shaders/DeferredLineVertex.hlsl";
    description.fragmentShaderPath = "assets/shaders/DeferredLineFragment.hlsl";
    description.vertexStride = sizeof(DeferredLineVertex);
    description.vertexAttributes = attributes;
    description.colorTargets = {
        {.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, .blend = blend}};
    description.depth.hasDepthTarget = false;
    description.primitiveType = SDL_GPU_PRIMITIVETYPE_LINELIST;
    s_LineState = std::make_unique<PipelineState>(description);
    s_LineBatch = std::make_unique<BatchBuffer>(
        "DeferredLine", (uint32_t)sizeof(DeferredLineVertex), 2 * maxLines,
        2 * maxLines, BatchBuffer::DrawOrder::Submission);
}

static const std::vector<uint32_t> s_LineIndices = {0, 1};

void DeferredRenderer::DrawLine(const glm::vec3 &start, const glm::vec3 &end,
                                const glm::vec4 &color) {
    std::vector<DeferredLineVertex> vertices = {
        {color, glm::vec4(start, 1)}, {color, glm::vec4(end, 1)}};
    s_LineBatch->Queue(vertices, s_LineIndices, nullptr);
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
    s_LineBatch->Queue(vertices, indices, nullptr);
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
                                const float normalStrength,
                                const Ref<DeferredGPassShader> &gPassShader) {
    DeferredGPassShader *shader =
        gPassShader != nullptr ? gPassShader.get() : s_DefaultGPassShader.get();
    if (shader == nullptr || !shader->IsValid())
        return;
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
        shader->GetBatch().Queue(vertices, QuadIndices,
                                 DefaultRenderer::s_WhiteTexture);
    else
        shader->GetBatch().Queue(vertices, QuadIndices, bindable);
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
    s_LightingBatch->Queue(vertices, QuadIndices, s_GBufferMaterial);
}

} // namespace Pyxis
