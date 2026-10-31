#pragma once

#include <Core/Core.h>
#include <SDL3/SDL_gpu.h>

namespace Pyxis {

// UploadQueue is a buffer of pending writes to textures or buffers.
//
// This is so they can all be written in a single copy pass, instead of
// possibly many small passes in a single frame. Huge time saver
class UploadBuffer {
    SDL_GPUDevice *m_GPUDevice = nullptr;
    // sdl expects uint32_t for buffer size creation
    uint32_t m_BufferSize = sizeof(uint32_t) * 1024u * 1024u;
    // raw memory to write to as buffer. will just use transfer buffer
    SDL_GPUTransferBuffer *m_TransferBuffer = nullptr;
    uint8_t *m_MappedMemory = nullptr;
    // index currently at.
    // not sure if this should be size_t...
    uint32_t m_BufferOffset = 0;

    // we only cycle once we reset the index to the start. This way,
    // we can use the same memory up until its full. allows for multiple
    // flushes in one frame to not get a bunch of new cycle allocations
    bool m_CycleNextMap = true;

    struct Upload {
        uint32_t uploadOffset = 0;
        bool cycle = false;

        // point to underlying data, there are only two options
        // if isTexture, do teture upload. if not, do buffer
        bool isTexture = false;
        SDL_GPUTexture *texture = nullptr;
        uint32_t x = 0, y = 0, w = 0, h = 0;

        SDL_GPUBuffer *buffer = nullptr;
        uint32_t destOffset = 0, size = 0;
    };

    // list of pending uploads that are queued right now
    std::vector<Upload> m_Uploads;

  public:
    UploadBuffer(SDL_GPUDevice *device);
    ~UploadBuffer();

    // uploads texture data to the ring / upload buffer, and will git actually
    // uploaded later. srcWidth allows you to specify the width if using a
    // smaller region of another texture / source
    void UploadTextureRegion(SDL_GPUTexture *texture, int x, int y, int w,
                             int h, void *pixels, int srcWidth = 0,
                             uint32_t bytesPerPixel = 4);

    // gives you the memory address to write your buffer info into. Do so
    // immediately!
    void *AllocateBufferUpload(SDL_GPUBuffer *buffer, uint32_t destOffset,
                               uint32_t size, bool cycle);

    // adds a copy pass to the buffer, and clears the pending uploads.
    // Does not necessarily cycle the upload buffer, that happens when it fills.
    void RunCopyPass(SDL_GPUCommandBuffer *cmdBuffer);

    // Flush is the early submit / runcopypass that is called when the buffer
    // hits a limit of size. This will create its own command buffer and submit
    // it if we aren't mid frame.
    void Flush();

  private:
    uint8_t *Allocate(uint32_t size, uint32_t alignment, uint32_t &out_offset);
    void Map();
    void Unmap();
};
} // namespace Pyxis
