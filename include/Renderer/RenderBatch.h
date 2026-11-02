#pragma once
#include <Renderer/Texture.h>

namespace Pyxis {
// RenderBatch is the class that holds the queued data to be drawn in a pass
// using a pipeline
class RenderBatch {
  public:
    enum DrawOrder { Submission, Depth };
    struct QueuedDraw {
        Ref<Bindable> bindable;
        float sortDepth = 0.0f;
        uint32_t vertexStart = 0, vertexCount = 0;
        uint32_t indexStart = 0, indexCount = 0;
        bool hasScissor = false;
        glm::vec4 scissor;
    };
    struct DrawCall {
        Bindable *bindable = nullptr;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        bool hasScissor = false;
        glm::vec4 scissor;
    };

    //////////////////////
    /// Main Functions ///
    //////////////////////

    RenderBatch(SDL_GPUDevice *gpuDevice, uint32_t vertexSize,
                uint32_t maxVertices = 10000 * 4,
                uint32_t maxIndices = 10000 * 6);
    ~RenderBatch();

    void SetScissor(const glm::vec4 &scissor);
    void ClearScissor();

    // Queues a mesh into the RenderBatch. sortDepth is only
    // used with DrawOrder::Depth
    template <typename VertexType>
    inline void Queue(const std::vector<VertexType> &vertices,
                      const std::vector<uint32_t> &indices,
                      Ref<Bindable> bindable, float sortDepth = 0.0f) {
        PX_ASSERT(sizeof(VertexType) == m_VertexSize,
                  "Attempting to queue mesh with wrong vertex size!");
        Queue(vertices.data(), (uint32_t)vertices.size(), indices.data(),
              (uint32_t)indices.size(), std::move(bindable), sortDepth);
    }
    void Queue(const void *vertexData, uint32_t vertexCount,
               const uint32_t *indices, uint32_t indexCount,
               Ref<Bindable> bindable, float sortDepth = 0.0f);

    // Clears the queued draws and scissor
    void Clear();

    std::vector<DrawCall> GetDrawCommands();

    friend class RenderPass;

  private:
    // properties of this batch
    DrawOrder m_RenderingOrder = Submission;
    uint32_t m_VertexSize = 0;
    uint32_t m_MaxVertices = 40000;
    uint32_t m_MaxIndices = 60000;

    // actual data to be held
    std::vector<uint8_t> m_Vertices;
    std::vector<uint32_t> m_Indices;
    SDL_GPUDevice *m_GPUDevice = nullptr;
    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    SDL_GPUBuffer *m_IndexBuffer = nullptr;

    // list of queued draws
    std::vector<QueuedDraw> m_QueuedDraws;

    // variables holding state
    glm::vec4 m_Scissor = {0, 0, 1920, 1080};
    bool m_HasScissor = false;
};
} // namespace Pyxis
