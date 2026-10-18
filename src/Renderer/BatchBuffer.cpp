#include <Renderer/BatchBuffer.h>
#include <algorithm>
#include <cstring>
#include <numeric>

namespace Pyxis {

BatchBuffer::BatchBuffer(const std::string &name, uint32_t vertexStride,
                         uint32_t maxVertices, uint32_t maxIndices,
                         DrawOrder drawOrder)
    : m_Name(name), m_VertexStride(vertexStride), m_MaxVertices(maxVertices),
      m_MaxIndices(maxIndices), m_DrawOrder(drawOrder) {
    SDL_GPUDevice *device = Renderer::GetGPUDevice();

    SDL_GPUBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.size = maxVertices * vertexStride;
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    m_VertexBuffer = SDL_CreateGPUBuffer(device, &vertexBufferInfo);
    if (PX_VERIFY(m_VertexBuffer != nullptr,
                  "BatchBuffer {}: failed to create the vertex buffer: {}",
                  name, SDL_GetError()))
        SDL_SetGPUBufferName(device, m_VertexBuffer,
                             (name + "_vertices").c_str());

    SDL_GPUBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.size = maxIndices * (uint32_t)sizeof(uint32_t);
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    m_IndexBuffer = SDL_CreateGPUBuffer(device, &indexBufferInfo);
    if (PX_VERIFY(m_IndexBuffer != nullptr,
                  "BatchBuffer {}: failed to create the index buffer: {}", name,
                  SDL_GetError()))
        SDL_SetGPUBufferName(device, m_IndexBuffer,
                             (name + "_indices").c_str());
}

BatchBuffer::~BatchBuffer() { Release(); }

void BatchBuffer::Release() {
    SDL_GPUDevice *device = Renderer::GetGPUDevice();
    if (device != nullptr) {
        // both buffers are released (the old Pipeline leaked its index
        // buffer)
        if (m_VertexBuffer != nullptr) {
            Renderer::GetUploadQueue().CancelUploads(m_VertexBuffer);
            SDL_ReleaseGPUBuffer(device, m_VertexBuffer);
        }
        if (m_IndexBuffer != nullptr) {
            Renderer::GetUploadQueue().CancelUploads(m_IndexBuffer);
            SDL_ReleaseGPUBuffer(device, m_IndexBuffer);
        }
    }
    m_VertexBuffer = nullptr;
    m_IndexBuffer = nullptr;
    Clear();
}

void BatchBuffer::Queue(const void *vertexData, uint32_t vertexCount,
                        const uint32_t *indices, uint32_t indexCount,
                        Ref<Bindable> bindable, float sortDepth) {
    if (vertexCount == 0 || indexCount == 0)
        return;
    const uint32_t queuedVertices =
        (uint32_t)(m_Vertices.size() / m_VertexStride);
    if (queuedVertices + vertexCount > m_MaxVertices) {
        PX_WARN("BatchBuffer {}: too many vertices to draw! skipping.", m_Name);
        return;
    }
    if (m_Indices.size() + indexCount > m_MaxIndices) {
        PX_WARN("BatchBuffer {}: too many indices to draw! skipping.", m_Name);
        return;
    }

    Record record;
    record.bindable = std::move(bindable);
    record.sortDepth = sortDepth;
    record.vertexStart = queuedVertices;
    record.vertexCount = vertexCount;
    record.indexStart = (uint32_t)m_Indices.size();
    record.indexCount = indexCount;
    record.hasScissor = m_HasScissor;
    record.scissor = m_Scissor;
    m_Records.push_back(std::move(record));

    const uint8_t *bytes = static_cast<const uint8_t *>(vertexData);
    m_Vertices.insert(m_Vertices.end(), bytes,
                      bytes + (size_t)vertexCount * m_VertexStride);
    m_Indices.insert(m_Indices.end(), indices, indices + indexCount);
    m_Prepared = false;
}

void BatchBuffer::SetScissor(const ScissorRect &rect) {
    m_HasScissor = true;
    m_Scissor = rect;
}

void BatchBuffer::ClearScissor() {
    m_HasScissor = false;
    m_Scissor = {};
}

void BatchBuffer::Clear() {
    m_Vertices.clear();
    m_Indices.clear();
    m_Records.clear();
    m_Runs.clear();
    m_Prepared = false;
    ClearScissor();
}

const std::vector<BatchBuffer::DrawRun> &BatchBuffer::PrepareDraw() {
    if (m_Prepared)
        return m_Runs;
    m_Prepared = true;
    m_Runs.clear();
    if (m_Records.empty() || m_VertexBuffer == nullptr ||
        m_IndexBuffer == nullptr)
        return m_Runs;

    // 1. order: call order, or stable back-to-front by sortDepth
    m_DrawOrderScratch.resize(m_Records.size());
    std::iota(m_DrawOrderScratch.begin(), m_DrawOrderScratch.end(), 0u);
    if (m_DrawOrder == DrawOrder::BackToFront) {
        std::stable_sort(m_DrawOrderScratch.begin(), m_DrawOrderScratch.end(),
                         [this](uint32_t a, uint32_t b) {
                             return m_Records[a].sortDepth >
                                    m_Records[b].sortDepth;
                         });
    }

    // 2. write the vertices / indices in that order straight into the
    // upload ring (indices rebased to their final vertex position), merging
    // consecutive records with the same bindable + scissor into one run
    UploadQueue &uploadQueue = Renderer::GetUploadQueue();
    uint8_t *vertexDestination = static_cast<uint8_t *>(
        uploadQueue.AllocateBufferUpload(m_VertexBuffer, 0,
                                         (uint32_t)m_Vertices.size(), true));
    if (vertexDestination == nullptr)
        return m_Runs;
    uint32_t vertexCursor = 0;
    for (uint32_t recordIndex : m_DrawOrderScratch) {
        const Record &record = m_Records[recordIndex];
        std::memcpy(vertexDestination + (size_t)vertexCursor * m_VertexStride,
                    m_Vertices.data() +
                        (size_t)record.vertexStart * m_VertexStride,
                    (size_t)record.vertexCount * m_VertexStride);
        vertexCursor += record.vertexCount;
    }

    // (the vertex bytes are written before the next allocation, which may
    // flush them if the ring is full)
    uint32_t *indexDestination =
        static_cast<uint32_t *>(uploadQueue.AllocateBufferUpload(
            m_IndexBuffer, 0, (uint32_t)(m_Indices.size() * sizeof(uint32_t)),
            true));
    if (indexDestination == nullptr)
        return m_Runs;
    vertexCursor = 0;
    uint32_t indexCursor = 0;
    for (uint32_t recordIndex : m_DrawOrderScratch) {
        const Record &record = m_Records[recordIndex];
        for (uint32_t i = 0; i < record.indexCount; i++)
            indexDestination[indexCursor + i] =
                m_Indices[record.indexStart + i] + vertexCursor;

        DrawRun *previous = m_Runs.empty() ? nullptr : &m_Runs.back();
        if (previous != nullptr &&
            previous->bindable == record.bindable.get() &&
            previous->hasScissor == record.hasScissor &&
            (!record.hasScissor || previous->scissor == record.scissor)) {
            previous->indexCount += record.indexCount;
        } else {
            DrawRun run;
            run.bindable = record.bindable.get();
            run.firstIndex = indexCursor;
            run.indexCount = record.indexCount;
            run.hasScissor = record.hasScissor;
            run.scissor = record.scissor;
            m_Runs.push_back(run);
        }
        vertexCursor += record.vertexCount;
        indexCursor += record.indexCount;
    }
    return m_Runs;
}

} // namespace Pyxis
