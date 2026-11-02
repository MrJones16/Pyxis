#include <Renderer/RenderBatch.h>

namespace Pyxis {

RenderBatch::RenderBatch(SDL_GPUDevice *gpuDevice, uint32_t vertexSize,
                         uint32_t maxVertices, uint32_t maxIndices)
    : m_GPUDevice(gpuDevice), m_VertexSize(vertexSize),
      m_MaxVertices(maxVertices), m_MaxIndices(maxIndices) {
    SDL_GPUBufferCreateInfo vertexBufferInfo;
    vertexBufferInfo.size = vertexSize * maxVertices;
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_VertexBuffer = SDL_CreateGPUBuffer(m_GPUDevice, &vertexBufferInfo);

    SDL_GPUBufferCreateInfo indexBufferInfo;
    indexBufferInfo.size = maxIndices * sizeof(uint32_t);
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    m_IndexBuffer = SDL_CreateGPUBuffer(m_GPUDevice, &indexBufferInfo);
}
RenderBatch::~RenderBatch() {
    SDL_ReleaseGPUBuffer(m_GPUDevice, m_VertexBuffer);
    m_VertexBuffer = nullptr;
    SDL_ReleaseGPUBuffer(m_GPUDevice, m_IndexBuffer);
    m_IndexBuffer = nullptr;
}

void RenderBatch::SetScissor(const glm::vec4 &scissor) {
    m_Scissor = scissor;
    m_HasScissor = true;
}
void RenderBatch::ClearScissor() {
    m_Scissor = {0, 0, 0, 0};
    m_HasScissor = false;
}

void RenderBatch::Queue(const void *vertexData, uint32_t vertexCount,
                        const uint32_t *indices, uint32_t indexCount,
                        Ref<Bindable> bindable, float sortDepth) {
    if (vertexCount == 0 || indexCount == 0)
        return;
    const uint32_t CurrentVertexCount =
        (uint32_t)(m_Vertices.size() / m_VertexSize);
    if (CurrentVertexCount + vertexCount > m_MaxVertices) {
        PX_WARN("BatchBuffer: too many vertices to draw! skipping.");
        return;
    }
    QueuedDraw draw{
        .bindable = bindable,
        .sortDepth = sortDepth,
        .vertexStart = CurrentVertexCount,
        .vertexCount = vertexCount,
        .indexStart = static_cast<uint32_t>(m_Indices.size()),
        .indexCount = indexCount,
        .hasScissor = m_HasScissor,
        .scissor = m_Scissor,
    };
    m_QueuedDraws.push_back(draw);
    const uint8_t *bytes = static_cast<const uint8_t *>(vertexData);
    m_Vertices.insert(m_Vertices.end(), bytes,
                      bytes + (size_t)vertexCount * m_VertexSize);
    m_Indices.insert(m_Indices.end(), indices, indices + indexCount);
}

void RenderBatch::Clear() {
    m_QueuedDraws.clear();
    m_Vertices.clear();
    m_Indices.clear();
    ClearScissor();
}

} // namespace Pyxis
