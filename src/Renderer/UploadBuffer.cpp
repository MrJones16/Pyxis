#include <Renderer/Renderer.h>
#include <Renderer/UploadBuffer.h>

namespace Pyxis {

// texture uploads start on 512 byte boundaries (the strictest copy
// alignment among SDL's backends, D3D12), buffer uploads on 16
static constexpr uint32_t c_TextureUploadAlignment = 512;
static constexpr uint32_t c_BufferUploadAlignment = 16;

// a helper function to round up to the nearest next value of the alignment.
// Needed as some rendering uploads require byte alignment, so dumb
static uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}
UploadBuffer::UploadBuffer(SDL_GPUDevice *device) {
    PX_ASSERT(device, "GPUDevice is null! can't create uploadbuffer");
    m_GPUDevice = device;
    SDL_GPUTransferBufferCreateInfo info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = m_BufferSize};
    m_TransferBuffer = SDL_CreateGPUTransferBuffer(m_GPUDevice, &info);
    PX_ASSERT(m_TransferBuffer,
              "Failed to create the UploadBuffer's Transfer buffer! size: {}",
              m_BufferSize);
}
UploadBuffer::~UploadBuffer() {
    if (m_MappedMemory != nullptr)
        Unmap();
    if (m_TransferBuffer != nullptr) {
        SDL_ReleaseGPUTransferBuffer(m_GPUDevice, m_TransferBuffer);
        m_TransferBuffer = nullptr;
    }
}

void UploadBuffer::UploadTextureRegion(SDL_GPUTexture *texture, int x, int y,
                                       int w, int h, void *pixels, int srcWidth,
                                       uint32_t bytesPerPixel) {
    if (pixels == nullptr)
        return;
    if (srcWidth == 0)
        srcWidth = w;

    // get the memory in the buffer and upload it to it
    uint32_t offset = 0;
    uint8_t *mem =
        Allocate(w * h * bytesPerPixel, c_TextureUploadAlignment, offset);
    for (int y = 0; y < h; y++) {
        std::memcpy(mem + (y * bytesPerPixel * w), pixels, w * bytesPerPixel);
    }

    // create the upload entry for later use to actually upload
    Upload textureUpload;
    // partial updates need to keep the rest of the texture i guess?
    textureUpload.isTexture = true;
    textureUpload.cycle = false;
    textureUpload.uploadOffset = offset;
    textureUpload.texture = texture;
    textureUpload.x = x;
    textureUpload.y = y;
    textureUpload.w = w;
    textureUpload.h = h;
    m_Uploads.push_back(textureUpload);
}
void *UploadBuffer::AllocateBufferUpload(SDL_GPUBuffer *buffer,
                                         uint32_t destOffset, uint32_t size,
                                         bool cycle) {
    // TODO
    return nullptr;
}

void UploadBuffer::RunCopyPass(SDL_GPUCommandBuffer *cmdBuffer) {
    if (cmdBuffer == nullptr || m_Uploads.empty())
        return;
    Unmap();
    auto copyPass = SDL_BeginGPUCopyPass(cmdBuffer);
    PX_ASSERT(copyPass, "Failed to begin copy pass!");

    for (auto &up : m_Uploads) {
        if (up.isTexture) { // uploading texture
            SDL_GPUTextureTransferInfo info;
            info.transfer_buffer = m_TransferBuffer;
            info.offset = up.uploadOffset;
            info.pixels_per_row = up.w;
            info.rows_per_layer = up.h;
            SDL_GPUTextureRegion region;
            region.layer = 0;
            region.texture = up.texture;
            region.w = up.w;
            region.h = up.h;
            region.x = up.x;
            region.y = up.y;
            SDL_UploadToGPUTexture(copyPass, &info, &region, up.cycle);
        } else { // uploading buffer
            SDL_GPUTransferBufferLocation location;
            location.transfer_buffer = m_TransferBuffer;
            location.offset = up.uploadOffset;

            SDL_GPUBufferRegion region;
            region.buffer = up.buffer;
            region.offset = up.destOffset;
            region.size = up.size;
            SDL_UploadToGPUBuffer(copyPass, &location, &region, up.cycle);
        }
    }
    SDL_EndGPUCopyPass(copyPass);
    m_Uploads.clear();
}

void UploadBuffer::Flush() {
    if (Renderer::IsFrameActive()) {
        // lets add to the current frame
        auto &frameData = Renderer::GetFrameData();
        RunCopyPass(frameData.GPUCommandBuffer);
    } else {
        auto cmdBuffer = Renderer::BeginGPUCommandBuffer();
        RunCopyPass(cmdBuffer);
        Renderer::EndGPUCommandBuffer(cmdBuffer);
    }
}

uint8_t *UploadBuffer::Allocate(uint32_t size, uint32_t alignment,
                                uint32_t &out_offset) {
    // if we are too big to even fit, lets double our size.
    if (size >= m_BufferSize) {
        Flush();
        m_BufferSize *= 2;
        if (m_TransferBuffer != nullptr) {
            SDL_ReleaseGPUTransferBuffer(m_GPUDevice, m_TransferBuffer);
            m_TransferBuffer = nullptr;
        }
        SDL_GPUTransferBufferCreateInfo info{
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = m_BufferSize};
        m_TransferBuffer = SDL_CreateGPUTransferBuffer(m_GPUDevice, &info);
        PX_ASSERT(
            m_TransferBuffer,
            "Failed to create the UploadBuffer's Transfer buffer! size: {}",
            m_BufferSize);
    }
    // we aren't in need of doubling our size, but are we at capacity?
    uint32_t newOffset = AlignUp(m_BufferOffset, alignment);
    if ((newOffset + size) > m_BufferSize) {
        // we hit capacity, lets flush and then add it
        Flush();
        Unmap();
        m_BufferOffset = 0;
        m_CycleNextMap = true;
        newOffset = 0;
    }
    Map();
    if (m_MappedMemory == nullptr)
        return nullptr;
    out_offset = newOffset;
    m_BufferOffset = newOffset + size;
    return m_MappedMemory + newOffset;
}
void UploadBuffer::Map() {
    if (m_MappedMemory != nullptr)
        return;
    m_MappedMemory = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(
        m_GPUDevice, m_TransferBuffer, m_CycleNextMap));
    PX_ASSERT(m_MappedMemory, "Failed to map memory!");
    m_CycleNextMap = false;
}
void UploadBuffer::Unmap() {
    if (m_MappedMemory == nullptr)
        return;
    SDL_UnmapGPUTransferBuffer(m_GPUDevice, m_TransferBuffer);
    m_MappedMemory = nullptr;
}
} // namespace Pyxis
