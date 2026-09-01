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

std::map<Renderer::SamplerType, SDL_GPUSampler *> Renderer::s_Samplers =
    std::map<SamplerType, SDL_GPUSampler *>();

bool Renderer::Init(const std::string &windowTitle, const glm::ivec2 resolution,
                    bool debug) {
    PX_TRACE("Initializing Renderer...");

    s_Window = SDL_CreateWindow(windowTitle.c_str(), resolution.x, resolution.y,
                                SDL_WINDOW_RESIZABLE);
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

    // if (!Texture::Init(s_GPUDevice)) {
    //    PX_ERROR("Error initializing texture samplers!");
    //    return false;
    //}

    s_FrameData = {};

    if (!TTF_Init()) {
        PX_ERROR("Failed to initialize SDL3_ttf: {}", SDL_GetError());
        return false;
    }

    PX_TRACE("Renderer Initialized!");

    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");

    // reverse order of init

    TTF_Quit();

    // release texture samplers
    // Texture::Shutdown(s_GPUDevice);
    for (auto &samplerkvp : s_Samplers) {
        SDL_ReleaseGPUSampler(s_GPUDevice, samplerkvp.second);
    }
    s_Samplers.clear();
    PX_TRACE("Texture Samplers Shut Down");

    SDL_ShaderCross_Quit();
    PX_TRACE("Shadercross Shut Down");

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

Renderer::FrameData &Renderer::BeginFrame() {

    PX_ASSERT(s_FrameData.GPUCommandBuffer == nullptr,
              "You already began a frame! Did you forget to end one?");

    // we want to begin GPU work, so get the command buffer
    s_FrameData.GPUCommandBuffer = BeginGPUCommandBuffer();

    // Acquire swapchain texture once per frame
    // This prevents multiple pipelines from trying to acquire the swapchain
    // texture, which would cause a deadlock
    s_FrameData.SwapchainTexture = nullptr;
    s_FrameData.SwapchainSize = {0, 0};
    s_FrameData.AcquiredSwapchain = true; // assume true

    bool success = SDL_AcquireGPUSwapchainTexture(
        s_FrameData.GPUCommandBuffer, s_Window, &s_FrameData.SwapchainTexture,
        (uint32_t *)&s_FrameData.SwapchainSize.x,
        (uint32_t *)&s_FrameData.SwapchainSize.y);
    if (!success || s_FrameData.SwapchainTexture == nullptr) {
        EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
        s_FrameData.AcquiredSwapchain = false;
        s_FrameData.GPUCommandBuffer = nullptr;
        s_FrameData.SwapchainTexture = nullptr;
        s_FrameData.SwapchainSize = {0, 0};
    }
    return s_FrameData;
}

void Renderer::EndFrame() {
    PX_ASSERT(s_FrameData.GPUCommandBuffer != nullptr,
              "You never began a pass!");
    EndGPUCommandBuffer(s_FrameData.GPUCommandBuffer);
    s_FrameData.GPUCommandBuffer = nullptr;
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
    PX_ASSERT(cmdBuffer != nullptr, "Failed to get a command buffer! {}",
              SDL_GetError());
    return cmdBuffer;
};

void Renderer::EndGPUCommandBuffer(SDL_GPUCommandBuffer *commandBuffer) {
    bool success = SDL_SubmitGPUCommandBuffer(commandBuffer);
    PX_ASSERT(success, "We failed to submit the command buffer!");
};

} // namespace Pyxis
