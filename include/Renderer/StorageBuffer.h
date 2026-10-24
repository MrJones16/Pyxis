#pragma once
#include <Renderer/Renderer.h>
#include <SDL3/SDL_gpu.h>

namespace Pyxis {

// StorageBuffer class which is higher level than the core renderer.
// Holds the root SDL_GPUBuffer, so all buffers must be destroyed before
// shutting down the renderer.
class StorageBuffer {
  protected:
    // name to reference it later when debugging
    std::string m_Name = "buffer";
    SDL_GPUBufferCreateInfo m_StorageBufferCreateInfo;
    SDL_GPUBuffer *m_StorageBuffer = nullptr;

  public:
    StorageBuffer(SDL_GPUBufferCreateInfo &bufferInfo,
                  const std::string &bufferName);
    ~StorageBuffer();

    // Create a specific buffer with advanced setup
    static Ref<StorageBuffer>
    CreateStorageBuffer(SDL_GPUBufferCreateInfo &bufferInfo,
                        const std::string &bufferName);

    inline SDL_GPUBuffer *GetGPUStorageBuffer() { return m_StorageBuffer; }

    // Sets the data in the buffer.
    void SetData(const void *data, uint32_t offset, uint32_t size = 0);

    // re-creates the buffer at a new size. Does not retain any data.
    void Resize(uint32_t size);

    // Binds the buffer to a slot for the provided render pass
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

  public:
};
} // namespace Pyxis
