#pragma once
#include <Core/Core.h>
#include <SDL3/SDL_gpu.h>
#include <vector>

namespace Pyxis {

// Per-frame GPU upload queue with a staging ring buffer, owned by the
// Renderer (see Renderer::GetUploadQueue).
//
// How it works:
// - One SDL transfer buffer is the staging ring. Every queued upload copies
//   its bytes into the ring right away, at the current offset (so the caller
//   may change or free its own memory immediately), and remembers where
//   the bytes go (a texture region or a buffer range).
// - RecordCopyPass records every pending upload into ONE copy pass on the
//   given command buffer (the frame's command buffer, before a render pass),
//   in the order they were queued. No command buffer is submitted for them.
// - Within a frame, later uploads keep appending after the bytes that the
//   recorded copy passes still read (the ring is only rewound after the
//   frame is submitted). OnFrameSubmitted rewinds to offset 0; the next map
//   then uses cycle = true, so SDL hands out a different backing buffer if
//   the GPU is still reading the previous frame's one. Nothing is
//   overwritten while in use.
// - If the ring is full, the pending uploads are flushed early through the
//   flush callback (the Renderer records them on the frame command buffer,
//   or submits a separate one when no frame is active) and the ring is
//   rewound. An upload bigger than the whole ring grows the ring.
class UploadQueue {
  public:
    // Called when the ring is full and pending uploads must be recorded now.
    using FlushFunction = void (*)();

    UploadQueue(SDL_GPUDevice *device, uint32_t initialCapacityBytes,
                FlushFunction flushWhenFull);
    ~UploadQueue();
    UploadQueue(const UploadQueue &) = delete;
    UploadQueue &operator=(const UploadQueue &) = delete;

    // Copies a w x h region of pixels into the ring and queues its upload
    // to (x, y) of `texture`. `sourceRowPitchBytes` is the distance between
    // rows in `sourcePixels`. The rest of the texture is preserved.
    void QueueTextureRegion(SDL_GPUTexture *texture, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, const void *sourcePixels,
                            uint32_t sourceRowPitchBytes,
                            uint32_t bytesPerPixel);

    // Reserves `size` bytes in the ring for an upload to `buffer` at
    // `destinationOffset` and returns where to write them (valid until the
    // next call into the queue). cycle = true lets SDL replace the GPU
    // buffer if it is still in use (the whole buffer content is discarded),
    // which is what per-frame vertex/index data wants.
    void *AllocateBufferUpload(SDL_GPUBuffer *buffer,
                               uint32_t destinationOffset, uint32_t size,
                               bool cycle);

    // Drops pending uploads that target a texture / buffer about to be
    // destroyed or recreated.
    void CancelUploads(SDL_GPUTexture *texture);
    void CancelUploads(SDL_GPUBuffer *buffer);

    bool HasPendingUploads() const { return !m_PendingUploads.empty(); }

    // Records all pending uploads into one copy pass on `commandBuffer`.
    // Must not be called inside a render pass.
    void RecordCopyPass(SDL_GPUCommandBuffer *commandBuffer);

    // The command buffer holding the recorded uploads was submitted: the
    // next upload starts again at the beginning of a (cycled) ring.
    void OnFrameSubmitted();

  private:
    struct PendingUpload {
        bool isTexture = true;
        uint32_t ringOffset = 0;
        // texture upload
        SDL_GPUTexture *texture = nullptr;
        uint32_t x = 0, y = 0, w = 0, h = 0;
        // buffer upload
        SDL_GPUBuffer *buffer = nullptr;
        uint32_t destinationOffset = 0, size = 0;
        bool cycle = false;
    };

    // Returns a pointer into the mapped ring for `size` bytes at an offset
    // aligned to `alignment`; sets `ringOffset`.
    uint8_t *Allocate(uint32_t size, uint32_t alignment, uint32_t &ringOffset);
    void Map();
    void Unmap();
    void CreateRing(uint32_t capacityBytes);

    SDL_GPUDevice *m_Device = nullptr;
    FlushFunction m_FlushWhenFull = nullptr;
    SDL_GPUTransferBuffer *m_Ring = nullptr;
    uint32_t m_Capacity = 0;
    uint32_t m_Offset = 0;
    uint8_t *m_Mapped = nullptr;
    // true when the next map starts a new region of the ring (offset 0):
    // map with cycle = true so the GPU's in-flight copy is never touched
    bool m_CycleOnNextMap = true;
    std::vector<PendingUpload> m_PendingUploads;
};

} // namespace Pyxis
