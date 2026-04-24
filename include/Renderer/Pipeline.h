#pragma once

#include <Core/Core.h>
#include <Renderer/Material.h>
#include <SDL3/SDL_gpu.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pyxis {

//////////  PIPELINES  //////////
// Pipelines are the abstraction over
// using a set of shaders, buffers,
// textures, ect, and queue the draw calls
// using their respective shaders.
//
// Create these from the renderer class
class Pipeline {
    friend class Renderer;

  protected:
    int m_Status = 0; // default, 0 is good,  otherwise bad.
    SDL_GPUDevice *m_Device = nullptr;
    SDL_GPUGraphicsPipeline *m_GraphicsPipeline;

    // vertex buffer
    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    uint32_t m_VertexSize = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_MaxSize = 0;

    // transfer buffer
    SDL_GPUTransferBuffer *m_TransferBuffer;
    void *m_TransferBufferData;
    SDL_GPUTransferBufferLocation m_TransferBufferLocation;

    // queues for materials
    std::unordered_map<Ref<Material>, std::vector<uint8_t>> m_MaterialBuffers;

    // output color targets
    std::vector<SDL_GPUColorTargetInfo> m_ColorTargetInfos;
    // whether or not we target screen output
    bool m_TargetSwapchain = false;

  public:
    Pipeline(SDL_GPUDevice *device, uint32_t maxVertices, uint32_t vertexSize,
             std::vector<SDL_GPUVertexAttribute> vertexAttributes,
             std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
             std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
             const std::string &vertexShaderPath,
             const std::string &fragmentShaderPath, bool TargetSwapchain);

    inline ~Pipeline() {
        SDL_ReleaseGPUBuffer(m_Device, m_VertexBuffer);
        SDL_ReleaseGPUTransferBuffer(m_Device, m_TransferBuffer);
        SDL_ReleaseGPUGraphicsPipeline(m_Device, m_GraphicsPipeline);
    }

    inline bool TargetsSwapchain() { return m_TargetSwapchain; };

    // maps the transfer buffer to a place we can write to.
    // Should be called at the beginning of a "frame", so that objects can write
    // to it.
    bool Map();
    void Unmap();

    // queue vertices to be drawn. material can be null if no materials/uniforms
    // are used!
    void QueueVertices(void *vertices, uint32_t count, Ref<Material> material);

    template <typename T>
    inline void QueueVertices(std::vector<T> &vertices,
                              Ref<Material> material) {
        PX_ASSERT(sizeof(T) == m_VertexSize,
                  "Drawing with incorrect vertex size!");
        uint8_t *bytes = (uint8_t *)vertices.data();
        m_MaterialBuffers[material].insert(
            m_MaterialBuffers[material].end(), bytes,
            bytes + (vertices.size() * m_VertexSize));
    }

    void UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer);

  private:
    void Bind(SDL_GPURenderPass *renderPass);
    void Draw(SDL_GPUCommandBuffer *commandBuffer, SDL_Window *window,
              SDL_GPUTexture *swapchainTexture);
};
} // namespace Pyxis
