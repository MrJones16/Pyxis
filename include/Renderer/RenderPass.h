#pragma once
#include <Core/Core.h>
#include <SDL3/SDL_gpu.h>
#include <string>
#include <vector>

namespace Pyxis {

class Texture;

// Where a render pass draws: the color attachments (in SV_Target order) and
// an optional depth attachment. Textures are referenced (not their SDL
// texture), so a Texture::Resize is picked up automatically.
struct RenderPassColorAttachment {
    // the target texture (needs SDL_GPU_TEXTUREUSAGE_COLOR_TARGET); ignored
    // when useSwapchain is set
    Ref<Texture> texture = nullptr;
    // draw to this frame's swapchain texture instead
    bool useSwapchain = false;
    SDL_GPULoadOp loadOp = SDL_GPU_LOADOP_CLEAR;
    SDL_GPUStoreOp storeOp = SDL_GPU_STOREOP_STORE;
    glm::vec4 clearColor = {0, 0, 0, 0};
};

struct RenderPassDepthAttachment {
    // needs SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    Ref<Texture> texture = nullptr;
    SDL_GPULoadOp loadOp = SDL_GPU_LOADOP_CLEAR;
    SDL_GPUStoreOp storeOp = SDL_GPU_STOREOP_STORE;
    float clearDepth = 1.0f; // far plane
    SDL_GPULoadOp stencilLoadOp = SDL_GPU_LOADOP_CLEAR;
    SDL_GPUStoreOp stencilStoreOp = SDL_GPU_STOREOP_STORE;
    uint8_t clearStencil = 0;
};

struct RenderPassDescription {
    std::string name = "pass";
    std::vector<RenderPassColorAttachment> colorAttachments;
    bool hasDepthAttachment = false;
    RenderPassDepthAttachment depthAttachment;
};

} // namespace Pyxis
