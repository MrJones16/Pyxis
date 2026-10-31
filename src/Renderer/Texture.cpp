#include "Core/Core.h"
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

Texture::Texture(SDL_GPUTextureCreateInfo &textureInfo,
                 const std::string &textureName, bool retainPixels)
    : m_TextureCreateInfo(textureInfo), m_Name(textureName),
      m_RetainPixelData(retainPixels) {
    m_Size = {m_TextureCreateInfo.width, m_TextureCreateInfo.height};
    if (m_RetainPixelData) {
        m_PixelData =
            std::vector<uint8_t>(m_Size.x * m_Size.y * m_BytesPerPixel);
    }

    PX_LOG("Creating texture {} with size {}", textureName, m_Size);
    SDL_GPUDevice *gpuDevice = Renderer::GetGPUDevice();

    m_Texture = SDL_CreateGPUTexture(gpuDevice, &m_TextureCreateInfo);
    PX_ASSERT(m_Texture != nullptr, "Failed to create texture! {}",
              SDL_GetError());
    SDL_SetGPUTextureName(gpuDevice, m_Texture, m_Name.c_str());
}

Texture::~Texture() {
    SDL_ReleaseGPUTexture(Renderer::GetGPUDevice(), m_Texture);
    m_Texture = nullptr;
}

Ref<Texture> Texture::CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                                    const std::string &textureName,
                                    bool retainPixels) {
    return CreateRef<Texture>(textureInfo, textureName, retainPixels);
}

Ref<Texture> Texture::CreateTexture(const glm::ivec2 &size,
                                    const std::string &textureName,
                                    bool retainPixels) {
    SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                     .format =
                                         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                     .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                     .width = (uint32_t)size.x,
                                     .height = (uint32_t)size.y,
                                     .layer_count_or_depth = 1,
                                     .num_levels = 1};
    return CreateRef<Texture>(info, textureName, retainPixels);
}

Ref<Texture> Texture::CreateTexture(const std::string &pngFilePath,
                                    const std::string &textureName,
                                    bool retainPixels) {
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    PX_ASSERT(surface != nullptr, "Failed to load PNG \"{}\"! {}", pngFilePath,
              SDL_GetError());
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    PX_ASSERT(convertedSurface != nullptr, "Failed to convert surface!");
    glm::ivec2 size = {surface->w, surface->h};
    Ref<Texture> texture = CreateTexture(size, textureName, retainPixels);
    texture->SetTextureData(surface->pixels);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(convertedSurface);
    return texture;
}

void Texture::Resize(const glm::ivec2 &size) {

    if (size == m_Size)
        return;
    auto device = Renderer::GetGPUDevice();
    SDL_ReleaseGPUTexture(device, m_Texture);

    m_Size = size;
    m_TextureCreateInfo.width = size.x;
    m_TextureCreateInfo.height = size.y;

    if (m_RetainPixelData) {
        m_PixelData =
            std::vector<uint8_t>(m_Size.x * m_Size.y * m_BytesPerPixel);
    }

    m_Texture =
        SDL_CreateGPUTexture(Renderer::GetGPUDevice(), &m_TextureCreateInfo);
    SDL_SetGPUTextureName(device, m_Texture, m_Name.c_str());
}

void Texture::SetTextureData(void *pixels) {
    SetTextureRegion(0, 0, m_Size.x, m_Size.y, pixels);
}

void Texture::SetTextureRegion(int x, int y, int w, int h, void *pixels,
                               int srcWidth) {
    // assume srcwidth matches w if it is not provided
    if (srcWidth == 0) {
        srcWidth = w;
    }

    size_t bytesPerRow = srcWidth * m_BytesPerPixel;
    const uint8_t *src = static_cast<const uint8_t *>(pixels);

    src += x * m_BytesPerPixel;
    src += y * bytesPerRow;

    if (m_RetainPixelData)
        for (int y = 0; y < h; y++) {
            std::memcpy(m_PixelData.data() + (x * m_BytesPerPixel) +
                            (y * m_BytesPerPixel * m_Size.x),
                        pixels, w * m_BytesPerPixel);
        }

    // Renderer::GetUploadQueue().QueueTextureRegion(...)
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
