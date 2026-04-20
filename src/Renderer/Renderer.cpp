#include <Renderer/Renderer.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

SDL_Window *Renderer::s_Window = nullptr;
SDL_GPUDevice *Renderer::s_GPUDevice = nullptr;
SDL_GPUCommandBuffer *Renderer::s_GPUCommandBuffer = nullptr;

std::vector<Pipeline *> Renderer::s_Pipelines = std::vector<Pipeline *>();

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

void Renderer::UnloadFont(int fontID) {
    Text::UnloadFont(fontID);
}

uint32_t Renderer::QueueText(int fontID, const std::string &text,
                             const glm::vec2 &position,
                             const glm::vec4 &color) {
    return Text::QueueText(fontID, text, position, color);
}

glm::ivec2 Renderer::GetTextSize(int fontID, const std::string &text) {
    return Text::GetTextSize(fontID, text);
}

} // namespace Pyxis
