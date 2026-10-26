#pragma once
#include <Renderer/Renderer.h>

namespace Pyxis {

class Bindable : public std::enable_shared_from_this<Bindable> {
  public:
    virtual void Bind(SDL_GPUCommandBuffer *cmbBuffer,
                      SDL_GPURenderPass *renderPass, int defaultSlot = 0) = 0;
};

// Texture class which is higher level than the core renderer.
// Holds the root SDL_GPUTexture, so all textures must be destroyed before
// shutting down the renderer.
class Texture : public Bindable {
  protected:
    // name to reference it later when debugging
    std::string m_Name = "texture";
    Renderer::SamplerType m_SamplerType = Renderer::PointWrap;
    SDL_GPUTextureCreateInfo m_TextureCreateInfo;
    SDL_GPUTexture *m_Texture = nullptr;
    glm::ivec2 m_Size;
    // bytes per texel of the texture's format (e.g. 4 for R8G8B8A8, 8 for
    // R16G16B16A16_FLOAT, 16 for R32G32B32A32_FLOAT)
    uint32_t m_BytesPerPixel = 4;

    // Optional CPU-side copy of the pixels (see "Pixel format" below).
    // Kept in sync by SetTextureData / SetTextureRegion when enabled. Only
    // available for 4 byte per pixel formats.
    bool m_RetainPixelData = false;
    std::vector<uint32_t> m_PixelData;

  public:
    Texture(SDL_GPUTextureCreateInfo &textureInfo,
            const std::string &textureName);
    ~Texture();

    // Create a specific texture with advanced setup
    static Ref<Texture> CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                                      const std::string &textureName);

    // Create a generic 2d blank texture with a set size
    static Ref<Texture> CreateTexture(const glm::ivec2 &size,
                                      const std::string &textureName);

    // Create a texture from a image.
    // By default a CPU-side copy of the pixels is kept so GetPixel /
    // SamplePixelTiled work (pass retainPixelData = false to skip it).
    static Ref<Texture> CreateTexture(const std::string &pngFilePath,
                                      const std::string &textureName,
                                      bool retainPixelData = true);

    inline SDL_GPUTexture *GetGPUTexture() { return m_Texture; }

    // Resizes the texture IF the provided size is different.
    // Recreates underlying texture, does not preserve any texture data
    void Resize(const glm::ivec2 &size);

    // Swaps the GPU textures of this and `other` (same format and size
    // required): every Material / pass holding either Ref<Texture> now sees
    // the other's pixels. Used to ping-pong full-screen passes that modify
    // a texture in place. Returns false (nothing swapped) on a mismatch.
    bool SwapGPUTexture(Texture &other);

    // Gets the size of the texture
    glm::ivec2 GetSize();
    // The texture's format and its size of one pixel in bytes
    inline SDL_GPUTextureFormat GetFormat() const {
        return m_TextureCreateInfo.format;
    }
    inline uint32_t GetBytesPerPixel() const { return m_BytesPerPixel; }

    // Pixel format
    // ------------
    // Pixel data is in the texture's own format, GetBytesPerPixel() bytes
    // per pixel. For the default SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM that
    // is 32 bits per pixel: bytes in memory are R, G, B, A
    // (SDL_PIXELFORMAT_RGBA32). Read as a little-endian uint32_t that is
    // 0xAABBGGRR, which is the same format as Element::color
    // (e.g. 0xFF00FFFF = opaque yellow). Rows are tightly packed, row 0 first
    // (row 0 is the top row of a loaded image).
    //
    // Uploads are queued
    // ------------------
    // SetTextureData / SetTextureRegion copy the pixels right away (you can
    // change or free `pixels` as soon as they return) into the Renderer's
    // upload queue. The GPU texture is updated by the copy pass recorded
    // before the next render pass of the frame (or at EndFrame), so a
    // texture changed during a frame is up to date for everything drawn
    // in it.

    // Sets the pixels in the texture. Assumes you are setting every pixel
    // (m_Size.x * m_Size.y pixels). Calls SetTextureRegion.
    void SetTextureData(const void *pixels);

    // Uploads only a sub-region (x, y, w, h) of the texture. `pixels` holds
    // the region's pixels; `srcRowLength` is the number of pixels per row in
    // `pixels` (0 = tightly packed, i.e. w). The region is clamped to the
    // texture; empty regions are ignored.
    void SetTextureRegion(int x, int y, int w, int h, const void *pixels,
                          int srcRowLength = 0);

    // Keep (or drop) a CPU-side copy of the pixel data. Enabling it does not
    // read back from the GPU: the copy starts zeroed and is filled by later
    // SetTextureData / SetTextureRegion calls.
    void SetRetainPixelData(bool retain);
    inline bool HasPixelData() const { return m_RetainPixelData; }

    // Reads a pixel from the CPU-side copy (0xAABBGGRR). Returns 0 when out of
    // bounds or when no CPU copy is retained.
    uint32_t GetPixel(int x, int y) const;

    // Like GetPixel but wraps coordinates (positive modulo), so the texture
    // tiles infinitely and negative coordinates work.
    uint32_t SamplePixelTiled(int x, int y) const;

    // Binds the texture to a slot for the provided render pass
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    // Binds the texture as a read-only fragment storage texture (no
    // sampler) to a storage texture slot. The texture needs
    // SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ. Used for integer formats
    // (e.g. R16_UINT ids) that are read with Load and can't be sampled.
    // HLSL: Texture2D<uint> declared after the sampled textures (no
    // matching sampler), see SDL_CreateGPUShader for the register order.
    void BindStorage(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    // override for the base class "Bindable", so that a draw can bind
    // either an entire material, or just a texture!
    void Bind(SDL_GPUCommandBuffer *cmbBuffer, SDL_GPURenderPass *renderPass,
              int defaultSlot = 0) override;

  public:
};
} // namespace Pyxis
