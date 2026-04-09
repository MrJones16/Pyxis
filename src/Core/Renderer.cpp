#include <Core/Renderer.h>
#include <SDL3/SDL_gpu.h>

namespace Pyxis {

static SDL_Window *s_Window = nullptr;
static SDL_GPUDevice *s_GPUDevice = nullptr;
static SDL_GPUCommandBuffer *s_GPUCommandBuffer = nullptr;

glm::ivec2 Renderer::s_Resolution = {1920, 1080};
glm::ivec2 Renderer::s_RenderResolution = {480, 270};
float Renderer::s_RenderPadding = 2;

bool Renderer::Init(const std::string &windowTitle,
                    const glm::ivec2 &resolution) {

    s_Resolution = resolution;
    s_RenderResolution = resolution;
    s_Window = SDL_CreateWindow(windowTitle.c_str(), resolution.x, resolution.y,
                                SDL_WINDOW_RESIZABLE);
    if (s_Window == nullptr) {
        PX_ERROR("Unable to initialize SDL Window : {}", SDL_GetError());
        return false;
    }

    s_GPUDevice = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
    if (s_GPUDevice == nullptr) {
        PX_ERROR("Error creating device: {}", SDL_GetError());
        return false;
    } else {
    }

    if (!SDL_ClaimWindowForGPUDevice(s_GPUDevice, s_Window)) {
        PX_ERROR("Error claiming window for gpu device: {}", SDL_GetError());
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");
    SDL_DestroyGPUDevice(s_GPUDevice);
    s_GPUDevice = nullptr;
    SDL_DestroyWindow(s_Window);
    s_Window = nullptr;
}

void Renderer::SetTitle(const std::string &title) {}
void Renderer::SetResolution(const glm::ivec2 &resolution) {
    SDL_SetWindowSize(s_Window, s_Resolution.x, s_Resolution.y);
}

void Renderer::BeginFrame() {
    PX_ASSERT(s_GPUCommandBuffer == nullptr, "You already began a frame!");

    // we want to begin GPU work, so get the command buffer
    s_GPUCommandBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    if (s_GPUCommandBuffer == nullptr) {
        PX_ERROR("Failed to get command buffer! {}", SDL_GetError());
        return;
    }

    SDL_GPUTexture *swapchainTexture;
    Uint32 width, height;
    SDL_WaitAndAcquireGPUSwapchainTexture(s_GPUCommandBuffer, s_Window,
                                          &swapchainTexture, &width, &height);

    SDL_GPUColorTargetInfo colorTargetInfo{};

    // discard previous content and clear to a color
    colorTargetInfo.clear_color = {255 / 255.0f, 219 / 255.0f, 187 / 255.0f,
                                   255 / 255.0f};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR; // or SDL_GPU_LOADOP_LOAD to
                                                    // keep the previous content

    // store the content to the texture
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    // where are we going to store the result?
    colorTargetInfo.texture = swapchainTexture; // we will set this later to the
                                                // window's swapchain texture

    // begin a render pass
    SDL_GPURenderPass *renderPass =
        SDL_BeginGPURenderPass(s_GPUCommandBuffer, &colorTargetInfo, 1, NULL);

    // end the render pass
    SDL_EndGPURenderPass(renderPass);
}

void Renderer::EndFrame() {
    PX_ASSERT(s_GPUCommandBuffer != nullptr, "You never began a pass!");
    // submit the command buffer to the GPU
    SDL_SubmitGPUCommandBuffer(s_GPUCommandBuffer);
    s_GPUCommandBuffer = nullptr;
}

class RenderPipeline {
    struct Vertex {
        glm::vec3 position;
        glm::vec4 color;
    };
    // a list of vertices
    Vertex vertices[3]{
        {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},   // top vertex
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // bottom left vertex
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}   // bottom right vertex
    };

  protected:
    SDL_GPUDevice *device;
    SDL_GPUBuffer *vertexBuffer;
    SDL_GPUTransferBuffer *transferBuffer;

  public:
    RenderPipeline(SDL_GPUDevice *gpudevice) {
        device = gpudevice;

        // create the vertex buffer
        SDL_GPUBufferCreateInfo bufferInfo{};
        bufferInfo.size = sizeof(vertices);
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vertexBuffer = SDL_CreateGPUBuffer(device, &bufferInfo);

        // create a transfer buffer to upload to the vertex buffer
        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.size = sizeof(vertices);
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

        // map the transfer buffer to a pointer
        Vertex *data =
            (Vertex *)SDL_MapGPUTransferBuffer(device, transferBuffer, false);

        data[0] = vertices[0];
        data[1] = vertices[1];
        data[2] = vertices[2];

        // or you can copy them all in one operation
        // SDL_memcpy(data, vertices, sizeof(vertices));

        // unmap the pointer when you are done updating the transfer buffer
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);
    }
    ~RenderPipeline() {
        SDL_ReleaseGPUBuffer(device, vertexBuffer);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }

    void Init() {
        // start a copy pass
        SDL_GPUCommandBuffer *commandBuffer =
            SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        // where is the data
        SDL_GPUTransferBufferLocation location{};
        location.transfer_buffer = transferBuffer;
        location.offset = 0; // start from the beginning

        // where to upload the data
        SDL_GPUBufferRegion region{};
        region.buffer = vertexBuffer;
        region.size = sizeof(vertices); // size of the data in bytes
        region.offset = 0;              // begin writing from the first vertex

        // upload the data
        SDL_UploadToGPUBuffer(copyPass, &location, &region, true);

        // end the copy pass
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(commandBuffer);
    }

    void PreRender() {}
};

} // namespace Pyxis
