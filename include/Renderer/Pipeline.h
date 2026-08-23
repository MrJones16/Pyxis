#pragma once

#include <Core/Core.h>
#include <Renderer/Material.h>
#include <SDL3/SDL_gpu.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pyxis {

struct DrawBuffer {
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indexData;
    void clear() {
        vertexData.clear();
        indexData.clear();
    }
};

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

    // uniforms that will be uploaded before render
    Ref<Uniform> m_VertexUniform = nullptr;
    Ref<Uniform> m_FragmentUniform = nullptr;

    // Depth & Stencil
    bool m_HasDepthStencilTexture = false;
    SDL_GPUDepthStencilTargetInfo m_DepthStencilTargetInfo{};

    // Vertex Buffer
    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    uint32_t m_VertexSize = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_MaxSize = 0;

    // Index Buffer
    SDL_GPUBuffer *m_IndexBuffer = nullptr;
    uint32_t m_MaxIndices = 0;
    uint32_t m_IndexCount = 0;

    // transfer buffers
    SDL_GPUTransferBuffer *m_VertexTransferBuffer;
    void *m_VertexTransferBufferData;
    SDL_GPUTransferBufferLocation m_VertexTransferBufferLocation;

    SDL_GPUTransferBuffer *m_IndexTransferBuffer;
    void *m_IndexTransferBufferData;
    SDL_GPUTransferBufferLocation m_IndexTransferBufferLocation;

    // queues for materials
    std::unordered_map<Ref<Material>, DrawBuffer> m_MaterialBuffers;

    // output color targets
    std::vector<SDL_GPUColorTargetInfo> m_ColorTargetInfos;
    // whether or not we target screen output
    bool m_TargetSwapchain = false;

    glm::ivec2 m_Resolution = {1920, 1080};

  public:
    Pipeline(SDL_GPUDevice *device, uint32_t maxVertices, uint32_t vertexSize,
             uint32_t maxIndices,
             std::vector<SDL_GPUVertexAttribute> vertexAttributes,
             std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
             std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
             SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo,
             const std::string &vertexShaderPath,
             const std::string &fragmentShaderPath, bool TargetSwapchain);

    inline ~Pipeline() {
        SDL_ReleaseGPUBuffer(m_Device, m_VertexBuffer);
        SDL_ReleaseGPUTransferBuffer(m_Device, m_VertexTransferBuffer);
        SDL_ReleaseGPUTransferBuffer(m_Device, m_IndexTransferBuffer);
        SDL_ReleaseGPUGraphicsPipeline(m_Device, m_GraphicsPipeline);
        m_VertexUniform = nullptr;
        m_FragmentUniform = nullptr;
    }

    inline bool TargetsSwapchain() { return m_TargetSwapchain; };

    // maps the transfer buffers to a place we can write to.
    bool Map();
    void Unmap();

    void SetVertexUniform(Ref<Uniform> uniform);
    void SetFragmentUniform(Ref<Uniform> uniform);

    void SetResolution(const glm::ivec2 &resolution);
    bool UpdateColorTargetTexture(int slot, const Ref<Texture> &texture);
    void UpdateDepthStencilTargetTexture(const Ref<Texture> &texture);

    inline std::vector<SDL_GPUColorTargetInfo> &GetColorTargets() {
        return m_ColorTargetInfos;
    }
    inline SDL_GPUDepthStencilTargetInfo &GetDepthStencilTarget() {
        return m_DepthStencilTargetInfo;
    }

    template <typename T>
    inline void QueueMesh(const std::vector<T> &vertices,
                          const std::vector<uint32_t> &indices,
                          Ref<Material> material) {
        PX_ASSERT(sizeof(T) == m_VertexSize,
                  "Drawing with incorrect vertex size!");
        // before copying vertices over, we need to know how many there are
        // first before
        uint32_t vertexCount =
            m_MaterialBuffers[material].vertexData.size() / m_VertexSize;
        // copy vertex data
        uint8_t *bytes = (uint8_t *)vertices.data();
        m_MaterialBuffers[material].vertexData.insert(
            m_MaterialBuffers[material].vertexData.end(), bytes,
            bytes + (vertices.size() * m_VertexSize));

        // copy indices to material buffer
        auto &indexVector = m_MaterialBuffers[material].indexData;
        for (auto i : indices) {
            // add vertex count to get correct index location
            indexVector.push_back(i + vertexCount);
        }
    }

    void UploadToGPU(SDL_GPUCommandBuffer *cmdBuffer);

  private:
    void Bind(SDL_GPURenderPass *renderPass);
    void Draw(SDL_GPUCommandBuffer *commandBuffer, SDL_Window *window,
              SDL_GPUTexture *swapchainTexture);
};
} // namespace Pyxis
