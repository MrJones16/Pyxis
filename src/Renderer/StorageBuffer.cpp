#include "Core/Core.h"
#include <Renderer/StorageBuffer.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>

namespace Pyxis {

StorageBuffer::StorageBuffer(SDL_GPUBufferCreateInfo &textureInfo,
                             const std::string &textureName)
    : m_StorageBufferCreateInfo(textureInfo), m_Name(textureName) {

    PX_LOG("Creating buffer {}", m_Name);
    SDL_GPUDevice *gpuDevice = Renderer::GetGPUDevice();

    m_StorageBuffer =
        SDL_CreateGPUBuffer(gpuDevice, &m_StorageBufferCreateInfo);
    if (PX_VERIFY(m_StorageBuffer != nullptr, "Failed to create Buffer {}: {}",
                  m_Name, SDL_GetError()))
        SDL_SetGPUBufferName(gpuDevice, m_StorageBuffer, m_Name.c_str());
    // no staging buffer per buffer. Uploads go through the Renderer's
    // upload queue (one shared staging ring)
}

StorageBuffer::~StorageBuffer() {
    if (m_StorageBuffer != nullptr && Renderer::GetGPUDevice() != nullptr) {
        Renderer::GetUploadQueue().CancelUploads(m_StorageBuffer);
        SDL_ReleaseGPUBuffer(Renderer::GetGPUDevice(), m_StorageBuffer);
    }
    m_StorageBuffer = nullptr;
}

Ref<StorageBuffer>
StorageBuffer::CreateStorageBuffer(SDL_GPUBufferCreateInfo &textureInfo,
                                   const std::string &textureName) {
    return CreateRef<StorageBuffer>(textureInfo, textureName);
}

void StorageBuffer::SetData(const void *data, uint32_t offset, uint32_t size) {
    auto destination = Renderer::GetUploadQueue().AllocateBufferUpload(
        m_StorageBuffer, 0, size, false);
    if (destination)
        std::memcpy(destination, data, size);
}

void StorageBuffer::Resize(uint32_t size) {
    m_StorageBufferCreateInfo.size = size;
    auto device = Renderer::GetGPUDevice();
    if (m_StorageBuffer != nullptr && device != nullptr) {
        Renderer::GetUploadQueue().CancelUploads(m_StorageBuffer);
        SDL_ReleaseGPUBuffer(device, m_StorageBuffer);
    }
    m_StorageBuffer = SDL_CreateGPUBuffer(device, &m_StorageBufferCreateInfo);
    if (PX_VERIFY(m_StorageBuffer != nullptr, "Failed to create Buffer {}: {}",
                  m_Name, SDL_GetError()))
        SDL_SetGPUBufferName(device, m_StorageBuffer, m_Name.c_str());
}

void StorageBuffer::Bind(SDL_GPURenderPass *renderPass, uint8_t slot) {
    SDL_BindGPUFragmentStorageBuffers(renderPass, slot, &m_StorageBuffer, 1);
}

} // namespace Pyxis
