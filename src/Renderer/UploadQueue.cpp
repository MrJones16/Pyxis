#include <Renderer/UploadQueue.h>
#include <algorithm>
#include <cstring>

namespace Pyxis {

// texture uploads start on 512 byte boundaries (the strictest copy
// alignment among SDL's backends, D3D12), buffer uploads on 16
static constexpr uint32_t c_TextureUploadAlignment = 512;
static constexpr uint32_t c_BufferUploadAlignment = 16;

static uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

UploadQueue::UploadQueue(SDL_GPUDevice *device, uint32_t initialCapacityBytes,
                         FlushFunction flushWhenFull)
    : m_Device(device), m_FlushWhenFull(flushWhenFull) {
    CreateRing(initialCapacityBytes);
}

UploadQueue::~UploadQueue() {
    Unmap();
    if (m_Ring != nullptr)
        SDL_ReleaseGPUTransferBuffer(m_Device, m_Ring);
}

void UploadQueue::CreateRing(uint32_t capacityBytes) {
    SDL_GPUTransferBufferCreateInfo info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = capacityBytes};
    m_Ring = SDL_CreateGPUTransferBuffer(m_Device, &info);
    PX_VERIFY(m_Ring != nullptr, "Failed to create the upload ring ({} bytes): {}",
              capacityBytes, SDL_GetError());
    m_Capacity = m_Ring != nullptr ? capacityBytes : 0;
    m_Offset = 0;
    m_CycleOnNextMap = true;
}

void UploadQueue::Map() {
    if (m_Mapped != nullptr || m_Ring == nullptr)
        return;
    m_Mapped = static_cast<uint8_t *>(
        SDL_MapGPUTransferBuffer(m_Device, m_Ring, m_CycleOnNextMap));
    PX_VERIFY(m_Mapped != nullptr, "Failed to map the upload ring: {}",
              SDL_GetError());
    m_CycleOnNextMap = false;
}

void UploadQueue::Unmap() {
    if (m_Mapped == nullptr)
        return;
    SDL_UnmapGPUTransferBuffer(m_Device, m_Ring);
    m_Mapped = nullptr;
}

uint8_t *UploadQueue::Allocate(uint32_t size, uint32_t alignment,
                               uint32_t &ringOffset) {
    uint32_t start = AlignUp(m_Offset, alignment);
    if ((uint64_t)start + size > m_Capacity) {
        // ring full: get the pending uploads recorded, then start over
        if (!m_PendingUploads.empty() && m_FlushWhenFull != nullptr)
            m_FlushWhenFull();
        Unmap();
        m_Offset = 0;
        m_CycleOnNextMap = true;
        start = 0;
        if (size > m_Capacity) {
            // bigger than the whole ring: grow it (SDL keeps the old buffer
            // alive until the GPU is done with it)
            uint32_t newCapacity = std::max(m_Capacity * 2, AlignUp(size, 1u << 20));
            PX_WARN("Upload ring grows from {} to {} bytes", m_Capacity,
                    newCapacity);
            if (m_Ring != nullptr)
                SDL_ReleaseGPUTransferBuffer(m_Device, m_Ring);
            CreateRing(newCapacity);
        }
    }
    Map();
    if (m_Mapped == nullptr)
        return nullptr;
    ringOffset = start;
    m_Offset = start + size;
    return m_Mapped + start;
}

void UploadQueue::QueueTextureRegion(SDL_GPUTexture *texture, uint32_t x,
                                     uint32_t y, uint32_t w, uint32_t h,
                                     const void *sourcePixels,
                                     uint32_t sourceRowPitchBytes,
                                     uint32_t bytesPerPixel) {
    if (texture == nullptr || sourcePixels == nullptr || w == 0 || h == 0)
        return;
    const uint32_t rowBytes = w * bytesPerPixel;
    uint32_t ringOffset = 0;
    uint8_t *destination =
        Allocate(rowBytes * h, c_TextureUploadAlignment, ringOffset);
    if (destination == nullptr)
        return;
    // tightly packed rows in the ring
    const uint8_t *source = static_cast<const uint8_t *>(sourcePixels);
    if (sourceRowPitchBytes == rowBytes) {
        std::memcpy(destination, source, (size_t)rowBytes * h);
    } else {
        for (uint32_t row = 0; row < h; row++)
            std::memcpy(destination + (size_t)rowBytes * row,
                        source + (size_t)sourceRowPitchBytes * row, rowBytes);
    }
    PendingUpload upload;
    upload.isTexture = true;
    upload.ringOffset = ringOffset;
    upload.texture = texture;
    upload.x = x;
    upload.y = y;
    upload.w = w;
    upload.h = h;
    m_PendingUploads.push_back(upload);
}

void *UploadQueue::AllocateBufferUpload(SDL_GPUBuffer *buffer,
                                        uint32_t destinationOffset,
                                        uint32_t size, bool cycle) {
    if (buffer == nullptr || size == 0)
        return nullptr;
    uint32_t ringOffset = 0;
    uint8_t *destination = Allocate(size, c_BufferUploadAlignment, ringOffset);
    if (destination == nullptr)
        return nullptr;
    PendingUpload upload;
    upload.isTexture = false;
    upload.ringOffset = ringOffset;
    upload.buffer = buffer;
    upload.destinationOffset = destinationOffset;
    upload.size = size;
    upload.cycle = cycle;
    m_PendingUploads.push_back(upload);
    return destination;
}

void UploadQueue::CancelUploads(SDL_GPUTexture *texture) {
    std::erase_if(m_PendingUploads, [texture](const PendingUpload &u) {
        return u.isTexture && u.texture == texture;
    });
}

void UploadQueue::CancelUploads(SDL_GPUBuffer *buffer) {
    std::erase_if(m_PendingUploads, [buffer](const PendingUpload &u) {
        return !u.isTexture && u.buffer == buffer;
    });
}

void UploadQueue::RecordCopyPass(SDL_GPUCommandBuffer *commandBuffer) {
    if (m_PendingUploads.empty() || commandBuffer == nullptr)
        return;
    // the ring must be unmapped while the GPU copies from it; later uploads
    // in this frame map it again (without cycling) and append after m_Offset
    Unmap();
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!PX_VERIFY(copyPass != nullptr, "Failed to begin a copy pass: {}",
                   SDL_GetError()))
        return;
    for (const PendingUpload &u : m_PendingUploads) {
        if (u.isTexture) {
            SDL_GPUTextureTransferInfo source{.transfer_buffer = m_Ring,
                                              .offset = u.ringOffset,
                                              .pixels_per_row = u.w,
                                              .rows_per_layer = u.h};
            SDL_GPUTextureRegion destination{.texture = u.texture,
                                             .x = u.x,
                                             .y = u.y,
                                             .w = u.w,
                                             .h = u.h,
                                             .d = 1};
            // cycle = false: partial updates must keep the rest of the
            // texture
            SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
        } else {
            SDL_GPUTransferBufferLocation source{.transfer_buffer = m_Ring,
                                                 .offset = u.ringOffset};
            SDL_GPUBufferRegion destination{.buffer = u.buffer,
                                            .offset = u.destinationOffset,
                                            .size = u.size};
            SDL_UploadToGPUBuffer(copyPass, &source, &destination, u.cycle);
        }
    }
    SDL_EndGPUCopyPass(copyPass);
    m_PendingUploads.clear();
}

void UploadQueue::OnFrameSubmitted() {
    Unmap();
    m_Offset = 0;
    m_CycleOnNextMap = true;
}

} // namespace Pyxis
