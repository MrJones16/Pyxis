#include "Core/Core.h"
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

Texture::Texture(SDL_GPUTextureCreateInfo &textureInfo,
                 const std::string &textureName)
    : m_TextureCreateInfo(textureInfo), m_Name(textureName) {
    m_Size = {m_TextureCreateInfo.width, m_TextureCreateInfo.height};
    m_BytesPerPixel =
        SDL_GPUTextureFormatTexelBlockSize(m_TextureCreateInfo.format);
    PX_LOG("Creating texture {} with size {}", m_Name, m_Size);

    SDL_GPUDevice *gpuDevice = Renderer::GetGPUDevice();

    m_Texture = SDL_CreateGPUTexture(gpuDevice, &m_TextureCreateInfo);
    if (PX_VERIFY(m_Texture != nullptr, "Failed to create texture {}: {}",
                  m_Name, SDL_GetError()))
        SDL_SetGPUTextureName(gpuDevice, m_Texture, m_Name.c_str());
    // no staging buffer per texture: uploads go through the Renderer's
    // upload queue (one shared staging ring)
}

Texture::~Texture() {
    if (m_Texture != nullptr && Renderer::GetGPUDevice() != nullptr) {
        Renderer::GetUploadQueue().CancelUploads(m_Texture);
        SDL_ReleaseGPUTexture(Renderer::GetGPUDevice(), m_Texture);
    }
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
                                    const std::string &textureName,
                                    bool retainPixelData) {
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    if (!PX_VERIFY(surface != nullptr, "Failed to load PNG \"{}\"! {}",
                   pngFilePath, SDL_GetError()))
        return nullptr;
    // RGBA32 = bytes R,G,B,A in memory, matching R8G8B8A8_UNORM.
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!PX_VERIFY(convertedSurface != nullptr,
                   "Failed to convert PNG \"{}\": {}", pngFilePath,
                   SDL_GetError()))
        return nullptr;
    glm::ivec2 size = {convertedSurface->w, convertedSurface->h};
    Ref<Texture> texture = CreateTexture(size, textureName);
    texture->SetRetainPixelData(retainPixelData);
    texture->SetTextureRegion(0, 0, size.x, size.y, convertedSurface->pixels,
                              convertedSurface->pitch / 4);
    SDL_DestroySurface(convertedSurface);
    return texture;
}

void Texture::Resize(const glm::ivec2 &size) {

    if (size == m_Size)
        return;
    SDL_GPUDevice *device = Renderer::GetGPUDevice();
    if (m_Texture != nullptr) {
        // uploads queued for the old texture are dropped with it
        Renderer::GetUploadQueue().CancelUploads(m_Texture);
        SDL_ReleaseGPUTexture(device, m_Texture);
    }

    m_Size = size;
    m_TextureCreateInfo.width = size.x;
    m_TextureCreateInfo.height = size.y;

    m_Texture = SDL_CreateGPUTexture(device, &m_TextureCreateInfo);
    PX_VERIFY(m_Texture != nullptr,
              "Failed to re-create texture after resizing: {}", SDL_GetError());
    SDL_SetGPUTextureName(device, m_Texture, m_Name.c_str());

    if (m_RetainPixelData)
        m_PixelData.assign((size_t)m_Size.x * m_Size.y, 0);
}

glm::ivec2 Texture::GetSize() { return m_Size; }

void Texture::SetTextureData(const void *pixels) {
    SetTextureRegion(0, 0, m_Size.x, m_Size.y, pixels);
}

void Texture::SetTextureRegion(int x, int y, int w, int h, const void *pixels,
                               int srcRowLength) {
    if (srcRowLength <= 0)
        srcRowLength = w;
    const uint8_t *src = static_cast<const uint8_t *>(pixels);
    const size_t srcRowPitch = (size_t)srcRowLength * m_BytesPerPixel;

    // clamp the region to the texture, offsetting the source to match
    int x0 = std::max(x, 0), y0 = std::max(y, 0);
    int x1 = std::min(x + w, m_Size.x), y1 = std::min(y + h, m_Size.y);
    if (x1 <= x0 || y1 <= y0 || pixels == nullptr || m_Texture == nullptr)
        return;
    src += (size_t)(y0 - y) * srcRowPitch + (size_t)(x0 - x) * m_BytesPerPixel;
    w = x1 - x0;
    h = y1 - y0;
    const size_t rowBytes = (size_t)w * m_BytesPerPixel;

    // keep the CPU copy in sync (4 byte pixels only, see SetRetainPixelData)
    if (m_RetainPixelData) {
        for (int row = 0; row < h; row++)
            memcpy(&m_PixelData[(size_t)(y0 + row) * m_Size.x + x0],
                   src + (size_t)row * srcRowPitch, rowBytes);
    }

    // copied into the staging ring now, uploaded in the frame's copy pass
    Renderer::GetUploadQueue().QueueTextureRegion(
        m_Texture, (uint32_t)x0, (uint32_t)y0, (uint32_t)w, (uint32_t)h, src,
        (uint32_t)srcRowPitch, m_BytesPerPixel);
}

void Texture::SetRetainPixelData(bool retain) {
    if (retain && !PX_VERIFY(m_BytesPerPixel == sizeof(uint32_t),
                             "SetRetainPixelData needs a 4 byte per pixel "
                             "format (this one has {} bytes)",
                             m_BytesPerPixel))
        retain = false;
    m_RetainPixelData = retain;
    if (retain)
        m_PixelData.assign((size_t)m_Size.x * m_Size.y, 0);
    else
        std::vector<uint32_t>().swap(m_PixelData);
}

uint32_t Texture::GetPixel(int x, int y) const {
    if (!m_RetainPixelData || x < 0 || y < 0 || x >= m_Size.x || y >= m_Size.y)
        return 0;
    return m_PixelData[(size_t)y * m_Size.x + x];
}

uint32_t Texture::SamplePixelTiled(int x, int y) const {
    if (!m_RetainPixelData || m_Size.x <= 0 || m_Size.y <= 0)
        return 0;
    // positive modulo so negative coordinates wrap correctly
    int tx = ((x % m_Size.x) + m_Size.x) % m_Size.x;
    int ty = ((y % m_Size.y) + m_Size.y) % m_Size.y;
    return m_PixelData[(size_t)ty * m_Size.x + tx];
}

void Texture::Bind(SDL_GPURenderPass *renderPass, uint8_t slot) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = m_Texture, .sampler = Renderer::s_Samplers[m_SamplerType]};
    SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

void Texture::BindStorage(SDL_GPURenderPass *renderPass, uint8_t slot) {
    SDL_BindGPUFragmentStorageTextures(renderPass, slot, &m_Texture, 1);
}

void Texture::Bind(SDL_GPUCommandBuffer *cmdBuffer,
                   SDL_GPURenderPass *renderPass, int defaultSlot) {
    // command buffer is ignored for texture bind, but is needed for materials
    // for uniform buffers

    Bind(renderPass, defaultSlot);
};

} // namespace Pyxis
