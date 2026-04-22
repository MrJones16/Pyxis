#include "Core/Core.h"
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

std::map<SamplerType, SDL_GPUSampler *> Texture::s_Samplers =
    std::map<SamplerType, SDL_GPUSampler *>();

bool Texture::Init(SDL_GPUDevice *device) {
    // initialize samplers for textures
    SDL_GPUSamplerCreateInfo samplerInfoPointClamp{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    s_Samplers[PointClamp] =
        SDL_CreateGPUSampler(device, &samplerInfoPointClamp);

    SDL_GPUSamplerCreateInfo samplerInfoPointWrap{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    s_Samplers[PointWrap] = SDL_CreateGPUSampler(device, &samplerInfoPointWrap);

    SDL_GPUSamplerCreateInfo samplerInfoLinearClamp{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    s_Samplers[LinearClamp] =
        SDL_CreateGPUSampler(device, &samplerInfoLinearClamp);

    SDL_GPUSamplerCreateInfo samplerInfoLinearWrap{
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    s_Samplers[LinearWrap] =
        SDL_CreateGPUSampler(device, &samplerInfoLinearWrap);
    for (auto &kvp : s_Samplers) {
        PX_ERROR("Unable to create samplers: {}", SDL_GetError());
        return false;
    }
    return true;
}

void Texture::Shutdown(SDL_GPUDevice *device) {
    for (auto &samplerkvp : s_Samplers) {
        SDL_ReleaseGPUSampler(device, samplerkvp.second);
    }
    s_Samplers.clear();
}

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

void Texture::SetTextureData(SDL_GPUDevice *device,
                             SDL_GPUCommandBuffer *commandBuffer,
                             void *pixels) {
    uint32_t size = m_Size.x * m_Size.y * sizeof(uint32_t);
    SDL_GPUTransferBufferCreateInfo tbInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size};

    // Create transfer buffer
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(device, &tbInfo);
    PX_ASSERT(transfer, SDL_GetError());

    // Map + copy pixels
    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    PX_ASSERT(mapped, SDL_GetError());
    memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    // Upload via copy pass
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commandBuffer);
    PX_ASSERT(copy, SDL_GetError());
    SDL_GPUTextureTransferInfo ttInfo{.transfer_buffer = transfer,
                                      .pixels_per_row = (uint32_t)m_Size.x};
    SDL_GPUTextureRegion textureRegion{
        .texture = m_Texture,
        .w = (uint32_t)m_Size.x,
        .h = (uint32_t)m_Size.y,
    };
    SDL_UploadToGPUTexture(copy, &ttInfo, &textureRegion, false);
    SDL_EndGPUCopyPass(copy);
}

void Texture::Bind(SDL_GPURenderPass *renderPass, uint8_t slot) {
    SDL_GPUTextureSamplerBinding binding = {
        .texture = m_Texture, .sampler = s_Samplers[m_SamplerType]};
    SDL_BindGPUFragmentSamplers(renderPass, slot, &binding, 1);
}

} // namespace Pyxis
