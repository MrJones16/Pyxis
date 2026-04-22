#include "Core/Core.h"
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

Texture::Texture(SDL_GPUDevice *device, const glm::ivec2 &size,
                 const std::string &textureName)
    : m_Device(device) {
    m_Size = size;
    SDL_GPUTextureCreateInfo textureInfo{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (uint32_t)size.x,
        .height = (uint32_t)size.y,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER};

    m_Texture = SDL_CreateGPUTexture(m_Device, &textureInfo);
    if (m_Texture == nullptr) {
        PX_STEPFAILURE("Failed to create texture! {} ", SDL_GetError());
        return;
    }
    SDL_SetGPUTextureName(m_Device, m_Texture, textureName.c_str());
}

Texture::Texture(SDL_GPUDevice *device, SDL_GPUTextureCreateInfo &textureInfo,
                 const std::string &textureName)
    : m_Device(device) {
    m_Size = {textureInfo.width, textureInfo.height};

    m_Texture = SDL_CreateGPUTexture(m_Device, &textureInfo);
    if (m_Texture == nullptr) {
        PX_STEPFAILURE("Failed to create texture! {} ", SDL_GetError());
        return;
    }
    PX_STEPSUCCESS("Created texture {}!", textureName);
    SDL_SetGPUTextureName(m_Device, m_Texture, textureName.c_str());
}

Texture::~Texture() {
    if (m_Texture != nullptr)
        SDL_ReleaseGPUTexture(m_Device, m_Texture);
}
} // namespace Pyxis
