#include "Renderer/Texture.h"
#include <Renderer/Renderer.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

SDL_Window *Renderer::s_Window = nullptr;
SDL_GPUDevice *Renderer::s_GPUDevice = nullptr;
SDL_GPUCommandBuffer *Renderer::s_GPUCommandBuffer = nullptr;

std::vector<Pipeline *> Renderer::s_Pipelines = std::vector<Pipeline *>();

std::map<SamplerType, SDL_GPUSampler *> Renderer::s_Samplers =
    std::map<SamplerType, SDL_GPUSampler *>();
std::vector<Texture *> Renderer::s_Textures = std::vector<Texture *>();

glm::ivec2 Renderer::s_RenderResolution = {480, 270};
float Renderer::s_RenderPadding = 2;

bool Renderer::Init(const std::string &windowTitle,
                    const glm::ivec2 &resolution) {

    s_RenderResolution = resolution;
    s_Window = SDL_CreateWindow(windowTitle.c_str(), resolution.x, resolution.y,
                                SDL_WINDOW_RESIZABLE);
    if (s_Window == nullptr) {
        PX_ERROR("Unable to initialize SDL Window : {}", SDL_GetError());
        return false;
    }

    s_GPUDevice = SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(),
                                      true, nullptr);
    if (s_GPUDevice == nullptr) {
        PX_ERROR("Error creating device: {}", SDL_GetError());
        return false;
    } else {
    }

    if (!SDL_ClaimWindowForGPUDevice(s_GPUDevice, s_Window)) {
        PX_ERROR("Error claiming window for gpu device: {}", SDL_GetError());
        return false;
    }

    if (!SDL_ShaderCross_Init()) {
        PX_ERROR("Error initializing sdl3/shadercross!: {}", SDL_GetError());
        return false;
    }

    // Initialize text rendering system
    if (!Text::Init(s_GPUDevice)) {
        PX_ERROR("Error initializing text rendering system!");
        return false;
    }

    // initialize samplers for textures
    SDL_GPUSamplerCreateInfo samplerInfoPointClamp{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    s_Samplers[PointClamp] =
        SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointClamp);

    SDL_GPUSamplerCreateInfo samplerInfoPointWrap{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    s_Samplers[PointWrap] =
        SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointWrap);

    SDL_GPUSamplerCreateInfo samplerInfoLinearClamp{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    s_Samplers[LinearClamp] =
        SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearClamp);

    SDL_GPUSamplerCreateInfo samplerInfoLinearWrap{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    s_Samplers[LinearWrap] =
        SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearWrap);

    struct SpriteVertex {
        glm::vec3 position;
        glm::vec4 color;
    };

    std::vector<SDL_GPUVertexAttribute> vertexAttributes;
    vertexAttributes.push_back(SDL_GPUVertexAttribute{
        // a_position
        .location = 0,    // layout (location = 0) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, // vec3
        .offset = 0 // start from the first byte from current buffer position

    });
    vertexAttributes.push_back(SDL_GPUVertexAttribute{
        // a_color
        .location = 1,    // layout (location = 1) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, // vec3
        .offset = sizeof(float) * 3 // 4th float from current buffer position

    });

    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions;
    SDL_GPUColorTargetDescription ct1;
    ct1.blend_state.enable_blend = true;
    ct1.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ct1.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    ct1.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ct1.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct1.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ct1.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct1.format = Renderer::GetSwapchainTextureFormat();
    colorTargetDescriptions.push_back(ct1);

    SDL_GPUColorTargetInfo colorTargetInfo{};
    // discard previous content and clear to a color
    colorTargetInfo.clear_color = {255 / 255.0f, 219 / 255.0f, 187 / 255.0f,
                                   255 / 255.0f};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR; // or SDL_GPU_LOADOP_LOAD to
    // store the content to the texture
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
    // colorTargetInfo.texture = leave blank for swapchain target, set
    // otherwise;
    std::vector<SDL_GPUColorTargetInfo> vec;
    vec.push_back(colorTargetInfo);

    // Create default sprite pipeline as an example & default
    int defaultPipelineID = CreatePipeline(
        6 * 2000, sizeof(SpriteVertex), vertexAttributes,
        colorTargetDescriptions, vec, "assets/shaders/vertex.hlsl",
        "assets/shaders/fragment.hlsl", true);
    if (defaultPipelineID < 0) {
        PX_ERROR("Failed to init Renderer, Pipeline creation failed!");
        return false;
    }
    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");

    // Shutdown text system
    Text::Shutdown();

    // release pipelines
    while (s_Pipelines.size() > 0) {
        Pipeline *p = s_Pipelines.back();
        s_Pipelines.pop_back();
        delete p;
    }

    for (auto &samplerkvp : s_Samplers) {
        SDL_ReleaseGPUSampler(s_GPUDevice, samplerkvp.second);
    }
    s_Samplers.clear();

    SDL_ShaderCross_Quit();

    SDL_DestroyGPUDevice(s_GPUDevice);
    s_GPUDevice = nullptr;
    SDL_DestroyWindow(s_Window);
    s_Window = nullptr;
}

void Renderer::OnWindowResize(const glm::ivec2 &resolution) {} // todo

void Renderer::SetTitle(const std::string &title) {
    SDL_SetWindowTitle(s_Window, title.c_str());
}

void Renderer::SetResolution(const glm::ivec2 &resolution) {
    SDL_SetWindowSize(s_Window, resolution.x, resolution.y);
}

glm::vec2 Renderer::GetResolution() {
    int w, h;
    SDL_GetWindowSize(s_Window, &w, &h);
    return glm::vec2(w, h);
}

Ref<Texture> Renderer::CreateTexture(const std::string &pngFilePath,
                                     const std::string &textureName) {
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    if (surface == nullptr) {
        PX_STEPFAILURE("Failed to load PNG \"{}\"! {}", pngFilePath,
                       SDL_GetError());
        return nullptr;
    }
    glm::ivec2 size = {surface->w, surface->h};
    Ref<Texture> texture = CreateRef<Texture>(s_GPUDevice, size, textureName);
    UploadTextureData(texture, surface->pixels);
    return texture;
}
Ref<Texture> Renderer::CreateTexture(const glm::ivec2 &size,
                                     const std::string &textureName) {
    return CreateRef<Texture>(s_GPUDevice, size, textureName);
}
Ref<Texture> Renderer::CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                                     const std::string &textureName) {
    return CreateRef<Texture>(s_GPUDevice, textureInfo, textureName);
}

void Renderer::UploadTextureData(Ref<Texture> &texture, void *pixels) {
    // Confirming we don't already have a command buffer
    PX_ASSERT(s_GPUCommandBuffer == nullptr, SDL_GetError());

    // Create command buffer
    s_GPUCommandBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    PX_ASSERT(s_GPUCommandBuffer, SDL_GetError());

    uint32_t size = texture->m_Size.x * texture->m_Size.y * sizeof(uint32_t);
    SDL_GPUTransferBufferCreateInfo tbInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};

    // Create transfer buffer
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(s_GPUDevice, &tbInfo);
    PX_ASSERT(transfer, SDL_GetError());

    // Map + copy pixels
    void *mapped = SDL_MapGPUTransferBuffer(s_GPUDevice, transfer, false);
    PX_ASSERT(mapped, SDL_GetError());
    memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(s_GPUDevice, transfer);

    // Upload via copy pass
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(s_GPUCommandBuffer);
    PX_ASSERT(copy, SDL_GetError());
    SDL_GPUTextureTransferInfo ttInfo{.transfer_buffer = transfer,
                                      .pixels_per_row =
                                          (uint32_t)texture->m_Size.x};
    SDL_GPUTextureRegion textureRegion{
        .texture = texture->m_Texture,
        .w = (uint32_t)texture->m_Size.x,
        .h = (uint32_t)texture->m_Size.y,
    };
    SDL_UploadToGPUTexture(copy, &ttInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copy);
}

void Renderer::BindTexture(SDL_GPURenderPass *renderPass, Ref<Texture> &texture,
                           int slot) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = texture->m_Texture,
        .sampler = s_Samplers[texture->m_SamplerType]};
    SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

// void Renderer::DestroyTexture(Texture &t) {}

int Renderer::CreatePipeline(
    uint32_t maxVertices, uint32_t vertexSize,
    std::vector<SDL_GPUVertexAttribute> vertexAttributes,
    std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
    std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
    const std::string &vertexShaderPath, const std::string &fragmentShaderPath,
    bool targetSwapchain) {
    Pipeline *p =
        new Pipeline(s_GPUDevice, maxVertices, vertexSize, vertexAttributes,
                     colorTargetDescriptions, colorTargetInfos,
                     vertexShaderPath, fragmentShaderPath, targetSwapchain);

    if (p->m_Status != 0) {
        PX_ERROR("Failed to create pipeline!");
        return -1;
    }

    s_Pipelines.push_back(p);
    return s_Pipelines.size() - 1;
}

void Renderer::DrawPipeline(uint32_t pipelineIndex) {
    if (pipelineIndex >= s_Pipelines.size()) {
        PX_ERROR("Pipeline {} not found!", pipelineIndex);
        return;
    }
    Pipeline *p = s_Pipelines[pipelineIndex];

    if (p->TargetsSwapchain()) {
        SDL_GPUTexture *swapchainTexture;
        Uint32 width, height;
        SDL_WaitAndAcquireGPUSwapchainTexture(
            s_GPUCommandBuffer, s_Window, &swapchainTexture, &width, &height);
        p->m_ColorTargetInfos[0].texture = swapchainTexture;
    }

    p->Unmap();
    p->UploadToGPU(s_GPUCommandBuffer);

    // begin a render pass
    SDL_GPURenderPass *renderPass =
        SDL_BeginGPURenderPass(s_GPUCommandBuffer, p->m_ColorTargetInfos.data(),
                               p->m_ColorTargetInfos.size(), NULL);

    p->Bind(renderPass);
    p->Draw(renderPass);

    // end the render pass
    SDL_EndGPURenderPass(renderPass);
}

void Renderer::BeginFrame() {
    PX_ASSERT(s_GPUCommandBuffer == nullptr, "You already began a frame!");

    // we want to begin GPU work, so get the command buffer
    s_GPUCommandBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    if (s_GPUCommandBuffer == nullptr) {
        PX_ERROR("Failed to get command buffer! {}", SDL_GetError());
        return;
    }

    // map the pipelines so that they can be written to
    for (auto &pipeline : s_Pipelines) {
        pipeline->Map();
    }
}

void Renderer::EndFrame() {
    PX_ASSERT(s_GPUCommandBuffer != nullptr, "You never began a pass!");

    // submit the command buffer to the GPU
    SDL_SubmitGPUCommandBuffer(s_GPUCommandBuffer);
    s_GPUCommandBuffer = nullptr;
}

std::tuple<SDL_GPUTexture *, glm::ivec2> Renderer::GetSwapchainTexture() {
    uint32_t sizex, sizey;
    SDL_GPUTexture *swapchainTexture;
    SDL_WaitAndAcquireGPUSwapchainTexture(s_GPUCommandBuffer, s_Window,
                                          &swapchainTexture, &sizex, &sizey);
    return std::tuple<SDL_GPUTexture *, glm::ivec2>(swapchainTexture,
                                                    glm::ivec2(sizex, sizey));
}

SDL_GPUTextureFormat Renderer::GetSwapchainTextureFormat() {
    return SDL_GetGPUSwapchainTextureFormat(s_GPUDevice, s_Window);
}

// Text rendering API wrapper methods
int Renderer::LoadFont(const std::string &fontPath, uint32_t fontSize) {
    return Text::LoadFont(fontPath, fontSize);
}

void Renderer::UnloadFont(int fontID) { Text::UnloadFont(fontID); }

uint32_t Renderer::QueueText(int fontID, const std::string &text,
                             const glm::vec2 &position,
                             const glm::vec4 &color) {
    return Text::QueueText(fontID, text, position, color);
}

glm::ivec2 Renderer::GetTextSize(int fontID, const std::string &text) {
    return Text::GetTextSize(fontID, text);
}

} // namespace Pyxis
