#include "Core/Core.h"
#include "Renderer/Texture.h"
#include <Renderer/Renderer.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace Pyxis {

// Static member definitions
SDL_Window *Renderer::s_Window = nullptr;
SDL_GPUDevice *Renderer::s_GPUDevice = nullptr;
SDL_GPUCommandBuffer *Renderer::s_GPUCommandBuffer = nullptr;
SDL_GPUTexture *Renderer::s_SwapchainTexture = nullptr;
glm::ivec2 Renderer::s_SwapchainSize = {0, 0};
std::vector<Pipeline *> Renderer::s_Pipelines = std::vector<Pipeline *>();
std::vector<Texture *> Renderer::s_Textures = std::vector<Texture *>();
glm::ivec2 Renderer::s_RenderResolution = {480, 270};
float Renderer::s_RenderPadding = 2;

bool Renderer::Init(const std::string &windowTitle, const glm::ivec2 resolution,
                    bool debug) {
    PX_TRACE("Initializing Renderer...");

    s_RenderResolution = resolution;
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

    // Initialize Texture samplers
    if (!Texture::Init(s_GPUDevice)) {
        PX_ERROR("Error initializing texture samplers!");
        return false;
    }

    s_GPUCommandBuffer = nullptr;

    // Initialize text rendering system
    if (!Text::Init(s_GPUDevice)) {
        PX_ERROR("Error initializing text rendering system!");
        return false;
    }

    PX_TRACE("Renderer Initialized!");

    return true;
}

void Renderer::Shutdown() {
    PX_LOG("Shutting down renderer.");

    // reverse order of init

    // release pipelines
    while (s_Pipelines.size() > 0) {
        Pipeline *p = s_Pipelines.back();
        s_Pipelines.pop_back();
        delete p;
    }

    // Shutdown text system
    Text::Shutdown();

    // release texture samplers
    Texture::Shutdown(s_GPUDevice);

    SDL_ShaderCross_Quit();
    PX_TRACE("Shadercross Shut Down");

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
    PX_BEGINSTEPS("Renderer-> Creating texture {}", textureName);
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    if (surface == nullptr) {
        PX_STEPFAILURE("Failed to load PNG \"{}\"! {}", pngFilePath,
                       SDL_GetError());
        return nullptr;
    }
    PX_STEPSUCCESS("Loaded PNG");
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    PX_ASSERT(convertedSurface != nullptr, "Failed to convert surface!");
    glm::ivec2 size = {surface->w, surface->h};
    Ref<Texture> texture = CreateRef<Texture>(s_GPUDevice, size, textureName);
    PX_TRACE("  Going to upload texture data...");
    UploadTextureData(texture, surface->pixels);
    PX_STEPSUCCESS("Texture data uploaded!");
    SDL_DestroySurface(surface);
    SDL_DestroySurface(convertedSurface);
    return texture;
}

bool Renderer::BeginFrame() {

    PX_ASSERT(s_GPUCommandBuffer == nullptr,
              "You already began a frame! Did you forget to end one?");

    // we want to begin GPU work, so get the command buffer
    s_GPUCommandBuffer = SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    if (s_GPUCommandBuffer == nullptr) {
        PX_ERROR("Failed to get command buffer! {}", SDL_GetError());
        return false;
    }

    // Acquire swapchain texture once per frame
    // This prevents multiple pipelines from trying to acquire the swapchain
    // texture, which would cause a deadlock
    s_SwapchainTexture = nullptr;
    s_SwapchainSize = {0, 0};

    // TODO: Look at this later, determine if I like waiting or skipping and
    // returning false
    SDL_WaitAndAcquireGPUSwapchainTexture(
        s_GPUCommandBuffer, s_Window, &s_SwapchainTexture,
        (uint32_t *)&s_SwapchainSize.x, (uint32_t *)&s_SwapchainSize.y);
    if (s_SwapchainTexture == nullptr) {
        SDL_SubmitGPUCommandBuffer(s_GPUCommandBuffer);
        s_GPUCommandBuffer = nullptr;
        PX_TRACE("Skipping frame!");
        return false;
    }
    return true;
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

glm::ivec2 Renderer::GetTextSize(int fontID, const std::string &text) {
    return Text::GetTextSize(fontID, text);
}

} // namespace Pyxis
