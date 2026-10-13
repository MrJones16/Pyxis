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
                                    const std::string &textureName,
                                    bool retainPixelData) {
    // LOAD FILE
    SDL_Surface *surface = SDL_LoadPNG(pngFilePath.c_str());
    PX_ASSERT(surface != nullptr, "Failed to load PNG \"{}\"! {}", pngFilePath,
              SDL_GetError());
    // RGBA32 = bytes R,G,B,A in memory, matching R8G8B8A8_UNORM.
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    PX_ASSERT(convertedSurface != nullptr, "Failed to convert surface!");
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
    const uint32_t *src = static_cast<const uint32_t *>(pixels);

    // clamp the region to the texture, offsetting the source to match
    int x0 = std::max(x, 0), y0 = std::max(y, 0);
    int x1 = std::min(x + w, m_Size.x), y1 = std::min(y + h, m_Size.y);
    if (x1 <= x0 || y1 <= y0 || pixels == nullptr)
        return;
    src += (size_t)(y0 - y) * srcRowLength + (x0 - x);
    w = x1 - x0;
    h = y1 - y0;
    const size_t rowBytes = (size_t)w * sizeof(uint32_t);

    // keep the CPU copy in sync
    if (m_RetainPixelData) {
        for (int row = 0; row < h; row++)
            memcpy(&m_PixelData[(size_t)(y0 + row) * m_Size.x + x0],
                   src + (size_t)row * srcRowLength, rowBytes);
    }

    SDL_GPUDevice *device = Renderer::GetGPUDevice();

    // Map + copy pixels tightly packed. cycle = true so we never overwrite
    // data a previous, still pending upload is reading from.
    void *mapped = SDL_MapGPUTransferBuffer(device, m_TransferBuffer, true);
    PX_ASSERT(mapped, SDL_GetError());
    uint8_t *dst = static_cast<uint8_t *>(mapped);
    if (srcRowLength == w) {
        memcpy(dst, src, rowBytes * h);
    } else {
        for (int row = 0; row < h; row++)
            memcpy(dst + rowBytes * row, src + (size_t)row * srcRowLength,
                   rowBytes);
    }
    SDL_UnmapGPUTransferBuffer(device, m_TransferBuffer);

    // Upload via copy pass. cycle = false: the rest of the texture must be
    // preserved for partial updates.
    SDL_GPUCommandBuffer *cmdBuffer = Renderer::BeginGPUCommandBuffer();
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmdBuffer);
    PX_ASSERT(copy, SDL_GetError());
    SDL_GPUTextureTransferInfo ttInfo{.transfer_buffer = m_TransferBuffer,
                                      .offset = 0,
                                      .pixels_per_row = (uint32_t)w,
                                      .rows_per_layer = (uint32_t)h};
    SDL_GPUTextureRegion textureRegion{.texture = m_Texture,
                                       .x = (uint32_t)x0,
                                       .y = (uint32_t)y0,
                                       .w = (uint32_t)w,
                                       .h = (uint32_t)h,
                                       .d = 1};
    SDL_UploadToGPUTexture(copy, &ttInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copy);

    Renderer::EndGPUCommandBuffer(cmdBuffer);
}

void Texture::SetRetainPixelData(bool retain) {
    m_RetainPixelData = retain;
    if (retain)
        m_PixelData.assign((size_t)m_Size.x * m_Size.y, 0);
    else
        std::vector<uint32_t>().swap(m_PixelData);
}

uint32_t Texture::GetPixel(int x, int y) const {
    if (!m_RetainPixelData || x < 0 || y < 0 || x >= m_Size.x ||
        y >= m_Size.y)
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

void Texture::Bind(SDL_GPUCommandBuffer *cmdBuffer,
                   SDL_GPURenderPass *renderPass, int defaultSlot) {
    // command buffer is ignored for texture bind, but is needed for materials
    // for uniform buffers

    Bind(renderPass, defaultSlot);
};

} // namespace Pyxis
