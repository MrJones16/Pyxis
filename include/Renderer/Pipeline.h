#pragma once

#include <Core/Core.h>
#include <Renderer/Material.h>
#include <SDL3/SDL_gpu.h>
#include <string>
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
    SDL_GPUDevice *m_Device = nullptr;
    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    uint32_t m_VertexSize = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_MaxSize = 0;

    SDL_GPUTransferBuffer *m_TransferBuffer;
    void *m_TransferBufferData;
    SDL_GPUTransferBufferLocation m_TransferBufferLocation;

    std::vector<SDL_GPUColorTargetInfo> m_ColorTargetInfos;
    bool m_TargetSwapchain = false;

    SDL_GPUGraphicsPipeline *m_GraphicsPipeline;

    int m_Status = 0; // default, 0 is good,  otherwise bad

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

    template <typename T> inline void QueueVertices(std::vector<T> vertices) {
        // this should only be called after the renderer's frame was begun so
        // that it's mapped.
        if (m_TransferBufferData == nullptr) {
            PX_ERROR(
                "Tried to queue vertices on a pipeline that's not mapped!");
            return;
        } else if ((vertices.size() + m_VertexCount) * m_VertexSize >
                   m_MaxSize) {
            PX_ERROR("Unable to queue more vertices, limit reached!");
            return;
        }
        T *transferBuffer = (T *)m_TransferBufferData;
        for (uint32_t i = 0; i < vertices.size(); i++) {
            transferBuffer[m_VertexCount + i] = vertices[i];
        }
        m_VertexCount += vertices.size();
    }

    void UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer);

  private:
    void Bind(SDL_GPURenderPass *renderPass);
    void Draw(SDL_GPURenderPass *renderPass);
};
} // namespace Pyxis
