#include "Core/Core.h"
#include <Renderer/Renderer.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace Pyxis {

// Static member definitions
SDL_Window *Renderer::s_Window = nullptr;
SDL_GPUDevice *Renderer::s_GPUDevice = nullptr;
Renderer::FrameData Renderer::s_FrameData = {};
std::unique_ptr<UploadQueue> Renderer::s_UploadQueue = nullptr;

// staging ring for all uploads; grows if a single upload is bigger
static constexpr uint32_t c_UploadRingCapacityBytes = 4u * 1024u * 1024u;

std::map<Renderer::SamplerType, SDL_GPUSampler *> Renderer::s_Samplers =
    std::map<SamplerType, SDL_GPUSampler *>();

bool Renderer::Init(const std::string &windowTitle, const glm::ivec2 resolution,
                    bool debug) {
    s_Window = SDL_CreateWindow(windowTitle.c_str(), resolution.x, resolution.y,
                                SDL_WINDOW_RESIZABLE);
    PX_BEGINSTEPS("Initializing Renderer...");
    if (s_Window == nullptr) {
        PX_ERROR("Unable to initialize SDL Window : {}", SDL_GetError());

        return false;
    }

    s_GPUDevice = SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(),
                                      debug, nullptr);
    if (s_GPUDevice == nullptr) {
        PX_ERROR("Error creating device: {}", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(s_GPUDevice, s_Window)) {
        PX_ERROR("Error claiming window for gpu device: {}", SDL_GetError());
        return false;
    }

    if (SDL_WindowSupportsGPUPresentMode(s_GPUDevice, s_Window,
                                         SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        SDL_SetGPUSwapchainParameters(s_GPUDevice, s_Window,
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      SDL_GPU_PRESENTMODE_IMMEDIATE);
    } else {
        PX_WARN("Unable to use IMMEDIATE mode!");
    }

    if (!SDL_ShaderCross_Init()) {
        PX_ERROR("Error initializing sdl3/shadercross!: {}", SDL_GetError());
        return false;
    }

    // Initialize default Texture samplers
    SDL_GPUSamplerCreateInfo samplerInfoPointClamp{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    auto sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointClamp);
    PX_ASSERT(sampler != nullptr, "Unable to create PointClamp Sampler!")
    s_Samplers[PointClamp] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoPointWrap{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };

    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoPointWrap);
    PX_ASSERT(sampler != nullptr, "Unable to create PointWrap Sampler!")
    s_Samplers[PointWrap] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoLinearClamp{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearClamp);
    PX_ASSERT(sampler != nullptr, "Unable to create LinearClamp Sampler!")
    s_Samplers[LinearClamp] = sampler;

    SDL_GPUSamplerCreateInfo samplerInfoLinearWrap{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    sampler = SDL_CreateGPUSampler(s_GPUDevice, &samplerInfoLinearWrap);
    PX_ASSERT(sampler != nullptr, "Unable to create LinearWrap Sampler!")
    s_Samplers[LinearWrap] = sampler;

    s_FrameData = {};

    s_UploadQueue = std::make_unique<UploadQueue>(
        s_GPUDevice, c_UploadRingCapacityBytes, &Renderer::FlushPendingUploads);

    if (!TTF_Init()) {
        PX_ERROR("Failed to initialize SDL3_ttf: {}", SDL_GetError());
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");

    // reverse order of init

    TTF_Quit();

    // pending uploads are dropped (nothing will draw with them anymore)
    s_UploadQueue = nullptr;

    // release texture samplers
    // Texture::Shutdown(s_GPUDevice);
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

//////////////////////
/// EVENT HANDLING ///
//////////////////////

// void Renderer::OnWindowResize(const glm::ivec2 &resolution) {} // todo

/////////////////////
/// GETS AND SETS ///
/////////////////////

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

//////////////////////
/// MAIN FUNCTIONS ///
//////////////////////

bool Renderer::BeginFrame() {
    if (!PX_VERIFY(s_FrameData.GPUCommandBuffer == nullptr,
                   "BeginFrame: a frame was already begun (missing EndFrame), "
                   "ending it first"))
        EndFrame();

    // we want to begin GPU work, so get the command buffer
    s_FrameData = {};
    s_FrameData.GPUCommandBuffer = BeginGPUCommandBuffer();
    if (s_FrameData.GPUCommandBuffer == nullptr)
        return false;

    // Acquire swapchain texture once per frame
    // This prevents multiple pipelines from trying to acquire the swapchain
    // texture, which would cause a deadlock
    uint32_t width = 0, height = 0;
    bool success = SDL_AcquireGPUSwapchainTexture(
        s_FrameData.GPUCommandBuffer, s_Window, &s_FrameData.SwapchainTexture,
        &width, &height);
    s_FrameData.SwapchainSize = {(int)width, (int)height};
    if (!success || s_FrameData.SwapchainTexture == nullptr) {
        if (!success)
            PX_ERROR("Failed to acquire the swapchain texture: {}",
                     SDL_GetError());
        // no frame this time: still submit the queued uploads with it
        s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
        EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
        s_UploadQueue->OnFrameSubmitted();
        s_FrameData = {};
        return false;
    }
    s_FrameData.AcquiredSwapchain = true;
    return true;
}

void Renderer::EndFrame() {
    if (s_FrameData.GPUCommandBuffer == nullptr)
        return; // no frame was begun (or BeginFrame failed)
    // uploads queued after the last render pass
    s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
    EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
    s_UploadQueue->OnFrameSubmitted();
    s_FrameData = {};
}

bool Renderer::IsFrameActive() {
    return s_FrameData.GPUCommandBuffer != nullptr;
}

Renderer::FrameData &Renderer::GetFrameData() { return s_FrameData; }

UploadQueue &Renderer::GetUploadQueue() { return *s_UploadQueue; }

void Renderer::FlushPendingUploads() {
    if (s_UploadQueue == nullptr || !s_UploadQueue->HasPendingUploads())
        return;
    if (IsFrameActive()) {
        s_UploadQueue->RecordCopyPass(s_FrameData.GPUCommandBuffer);
        return;
    }
    // no frame to record into: use (and submit) a separate command buffer
    SDL_GPUCommandBuffer *commandBuffer = BeginGPUCommandBuffer();
    if (commandBuffer == nullptr)
        return;
    s_UploadQueue->RecordCopyPass(commandBuffer);
    EndGPUCommandBuffer(commandBuffer);
    s_UploadQueue->OnFrameSubmitted();
}

SDL_GPUTextureFormat Renderer::GetSwapchainTextureFormat() {
    return SDL_GetGPUSwapchainTextureFormat(s_GPUDevice, s_Window);
}

////////////////////////////////////////////////////////
/// PRIVATE FUNCTIONS TO BE USED BY TEXTURE/PIPELINE ///
////////////////////////////////////////////////////////

SDL_Window *Renderer::GetWindow() { return s_Window; };
SDL_GPUDevice *Renderer::GetGPUDevice() { return s_GPUDevice; };

SDL_GPUCommandBuffer *Renderer::BeginGPUCommandBuffer() {
    SDL_GPUCommandBuffer *cmdBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    PX_VERIFY(cmdBuffer != nullptr, "Failed to get a command buffer! {}",
              SDL_GetError());
    return cmdBuffer;
};

void Renderer::EndGPUCommandBuffer(SDL_GPUCommandBuffer *commandBuffer) {
    bool success = SDL_SubmitGPUCommandBuffer(commandBuffer);
    PX_VERIFY(success, "Failed to submit the command buffer: {}",
              SDL_GetError());
};

} // namespace Pyxis
