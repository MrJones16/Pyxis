#pragma once
#include <Renderer/Texture.h>
#include <string>
#include <vector>

namespace Pyxis {

// CPU-side list of meshes to draw with one PipelineState, plus the GPU
// vertex / index buffers they are uploaded to.
//
// Draw order (stable, explained):
// - Every Queue() call becomes a record, kept in the order of the calls.
// - DrawOrder::Submission (default): records are drawn in exactly that
//   order. DrawOrder::BackToFront: records are stable-sorted by their
//   `sortDepth` (larger = farther, drawn first; 0 = front, 1 = back like the
//   depth buffer), ties keep the order of the calls. Use it for
//   alpha-blended things that don't write depth (UI, transparent sprites).
// - Then consecutive records that use the same Bindable (texture /
//   material) and the same scissor rect are merged into one draw call.
//   Records are never reordered to merge more, so a different Bindable in
//   between splits the run: what you queue later always draws later.
// The result is the same every frame and every run (the old per-Bindable
// unordered_map drew in pointer-hash order).
//
// The vertex / index data is uploaded once per content change, through the
// Renderer's upload queue, when a render pass draws the batch. Clear() it
// after the pass (the renderers that own batches do this).
class BatchBuffer {
  public:
    enum class DrawOrder { Submission, BackToFront };

    // render target pixels, origin top left
    struct ScissorRect {
        int x = 0, y = 0, w = 0, h = 0;
        bool operator==(const ScissorRect &) const = default;
    };

    // one draw call, produced by PrepareDraw
    struct DrawRun {
        Bindable *bindable = nullptr;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        bool hasScissor = false;
        ScissorRect scissor;
    };

    BatchBuffer(const std::string &name, uint32_t vertexStride,
                uint32_t maxVertices, uint32_t maxIndices,
                DrawOrder drawOrder = DrawOrder::Submission);
    ~BatchBuffer();
    BatchBuffer(const BatchBuffer &) = delete;
    BatchBuffer &operator=(const BatchBuffer &) = delete;

    // Queues a mesh. `indices` index into `vertices` (0 = its first vertex).
    // `bindable` (texture or material) is bound for it; `sortDepth` is only
    // used with DrawOrder::BackToFront.
    template <typename VertexType>
    inline void Queue(const std::vector<VertexType> &vertices,
                      const std::vector<uint32_t> &indices,
                      Ref<Bindable> bindable, float sortDepth = 0.0f) {
        if (!PX_VERIFY(sizeof(VertexType) == m_VertexStride,
                       "BatchBuffer {}: vertex size {} doesn't match the "
                       "stride {}",
                       m_Name, sizeof(VertexType), m_VertexStride))
            return;
        Queue(vertices.data(), (uint32_t)vertices.size(), indices.data(),
              (uint32_t)indices.size(), std::move(bindable), sortDepth);
    }
    void Queue(const void *vertexData, uint32_t vertexCount,
               const uint32_t *indices, uint32_t indexCount,
               Ref<Bindable> bindable, float sortDepth = 0.0f);

    // Meshes queued from now on are clipped to `rect` (render target
    // pixels) until ClearScissor.
    void SetScissor(const ScissorRect &rect);
    void ClearScissor();

    // Removes all queued meshes (keeps the allocated memory).
    void Clear();
    inline bool IsEmpty() const { return m_Records.empty(); }
    inline uint32_t GetVertexStride() const { return m_VertexStride; }
    inline DrawOrder GetDrawOrder() const { return m_DrawOrder; }
    inline const std::string &GetName() const { return m_Name; }

    // Releases the GPU buffers now (before the device is destroyed).
    void Release();

    // Used by Renderer::EndPass: orders and merges the records into draw
    // runs and queues the upload of the vertex / index data (only when the
    // content changed since the last call).
    const std::vector<DrawRun> &PrepareDraw();
    inline SDL_GPUBuffer *GetGPUVertexBuffer() const { return m_VertexBuffer; }
    inline SDL_GPUBuffer *GetGPUIndexBuffer() const { return m_IndexBuffer; }

  private:
    struct Record {
        Ref<Bindable> bindable;
        float sortDepth = 0.0f;
        uint32_t vertexStart = 0, vertexCount = 0;
        uint32_t indexStart = 0, indexCount = 0;
        bool hasScissor = false;
        ScissorRect scissor;
    };

    std::string m_Name;
    uint32_t m_VertexStride = 0;
    uint32_t m_MaxVertices = 0;
    uint32_t m_MaxIndices = 0;
    DrawOrder m_DrawOrder = DrawOrder::Submission;

    // queued data: vertices as bytes, indices local to their record
    std::vector<uint8_t> m_Vertices;
    std::vector<uint32_t> m_Indices;
    std::vector<Record> m_Records;
    bool m_HasScissor = false;
    ScissorRect m_Scissor;

    // result of PrepareDraw, valid until the content changes
    bool m_Prepared = false;
    std::vector<DrawRun> m_Runs;
    std::vector<uint32_t> m_DrawOrderScratch;

    SDL_GPUBuffer *m_VertexBuffer = nullptr;
    SDL_GPUBuffer *m_IndexBuffer = nullptr;
};

} // namespace Pyxis
