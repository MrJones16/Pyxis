#include "Core/Core.h"
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

Texture::Texture(SDL_GPUTextureCreateInfo &textureInfo,
                 const std::string &textureName)
    : m_TextureCreateInfo(textureInfo) {
    m_Size = {m_TextureCreateInfo.width, m_TextureCreateInfo.height};
    PX_LOG("Creating texture {} with size {}", textureName, m_Size);

    SDL_GPUDevice *gpuDevice = Renderer::GetGPUDevice();

    m_Texture = SDL_CreateGPUTexture(gpuDevice, &m_TextureCreateInfo);
    PX_ASSERT(m_Texture != nullptr, "Failed to create texture! {}",
              SDL_GetError());
    SDL_SetGPUTextureName(gpuDevice, m_Texture, textureName.c_str());

    // Create transfer buffer
    uint32_t size = m_Size.x * m_Size.y * sizeof(uint32_t);
    SDL_GPUTransferBufferCreateInfo tbInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};
    m_TransferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &tbInfo);
    PX_ASSERT(m_TransferBuffer, SDL_GetError());
}

Texture::~Texture() {
    SDL_ReleaseGPUTexture(Renderer::GetGPUDevice(), m_Texture);
    SDL_ReleaseGPUTransferBuffer(Renderer::GetGPUDevice(), m_TransferBuffer);
    m_Texture = nullptr;
}

Ref<Texture> Texture::CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                                    const std::string &textureName) {
    return CreateRef<Texture>(textureInfo, textureName);
}

Ref<Texture> Texture::CreateTexture(const glm::ivec2 &size,
                                    const std::string &textureName) {
    SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                     .format =
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                     .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                     .width = (uint32_t)size.x,
                                     .height = (uint32_t)size.y,
                                     .layer_count_or_depth = 1,
                                     .num_levels = 1};
    return CreateRef<Texture>(info, textureName);
}

Ref<Texture> Texture::CreateTexture(const std::string &pngFilePath,
                                    const std::string &textureName) {
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    PX_ASSERT(surface != nullptr, "Failed to load PNG \"{}\"! {}", pngFilePath,
              SDL_GetError());
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    PX_ASSERT(convertedSurface != nullptr, "Failed to convert surface!");
    glm::ivec2 size = {surface->w, surface->h};
    Ref<Texture> texture = CreateTexture(size, textureName);
    texture->SetTextureData(surface->pixels);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(convertedSurface);
    return texture;
}

void Texture::Resize(const glm::ivec2 &size) {

    if (size == m_Size)
        return;
    SDL_ReleaseGPUTransferBuffer(Renderer::GetGPUDevice(), m_TransferBuffer);
    SDL_ReleaseGPUTexture(Renderer::GetGPUDevice(), m_Texture);

    m_Size = size;
    m_TextureCreateInfo.width = size.x;
    m_TextureCreateInfo.height = size.y;

    m_Texture =
        SDL_CreateGPUTexture(Renderer::GetGPUDevice(), &m_TextureCreateInfo);
    PX_ASSERT(m_Texture, "Failed to re-create texture after resizing: {}",
              SDL_GetError());

    // Create transfer buffer
    uint32_t dataSize = m_Size.x * m_Size.y * sizeof(uint32_t);
    SDL_GPUTransferBufferCreateInfo tbInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = dataSize};
    m_TransferBuffer =
        SDL_CreateGPUTransferBuffer(Renderer::GetGPUDevice(), &tbInfo);
    PX_ASSERT(m_TransferBuffer,
              "Failed to re-create transfer buffer after resize: {}",
              SDL_GetError());
}

glm::ivec2 Texture::GetSize() { return m_Size; }

void Texture::SetTextureData(void *pixels) {
    SDL_GPUDevice *device = Renderer::GetGPUDevice();
    SDL_GPUCommandBuffer *cmdBuffer = Renderer::BeginGPUCommandBuffer();

    // Map + copy pixels
    void *mapped = SDL_MapGPUTransferBuffer(device, m_TransferBuffer, false);
    PX_ASSERT(mapped, SDL_GetError());
    memcpy(mapped, pixels, sizeof(uint32_t) * m_Size.x * m_Size.y);
    SDL_UnmapGPUTransferBuffer(device, m_TransferBuffer);

    // Upload via copy pass
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmdBuffer);
    PX_ASSERT(copy, SDL_GetError());
    SDL_GPUTextureTransferInfo ttInfo{.transfer_buffer = m_TransferBuffer,
                                      .offset = 0};
    SDL_GPUTextureRegion textureRegion{.texture = m_Texture,
                                       .w = (uint32_t)m_Size.x,
                                       .h = (uint32_t)m_Size.y,
                                       .d = 1};
    SDL_UploadToGPUTexture(copy, &ttInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copy);

    Renderer::EndGPUCommandBuffer(cmdBuffer);
}

void Texture::Bind(SDL_GPURenderPass *renderPass, uint8_t slot) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = m_Texture, .sampler = Renderer::s_Samplers[m_SamplerType]};
    SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

void Texture::Bind(SDL_GPUCommandBuffer *cmdBuffer,
                   SDL_GPURenderPass *renderPass, int defaultSlot) {
    // command buffer is ignored for texture bind, but is needed for materials
    // for uniform buffers

    Bind(renderPass, defaultSlot);
};

} // namespace Pyxis
