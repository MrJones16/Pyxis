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
    std::string m_Name = "texture";
    Renderer::SamplerType m_SamplerType = Renderer::PointWrap;
    SDL_GPUTextureCreateInfo m_TextureCreateInfo;
    SDL_GPUTexture *m_Texture = nullptr;
    glm::ivec2 m_Size;

    // bytes per pixel of the texture's format (e.g. 4 for R8G8B8A8, 8 for
    // R16G16B16A16_FLOAT, 16 for R32G32B32A32_FLOAT)
    uint32_t m_BytesPerPixel = 4;

    // Optional CPU-side copy
    // Kept in sync by SetTextureData / SetTextureRegion when enabled.
    //
    // Only enable if you want to sample the texture in the CPU code
    const bool m_RetainPixelData = false;
    std::vector<uint8_t> m_PixelData;

  public:
    Texture(SDL_GPUTextureCreateInfo &textureInfo,
            const std::string &textureName, bool retainPixels = false);
    ~Texture();

    // Create a specific texture with advanced setup
    static Ref<Texture>
    CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                  const std::string &textureName = "texture",
                  bool retainPixels = false);

    // Create a generic 2d blank texture with a set size
    static Ref<Texture>
    CreateTexture(const glm::ivec2 &size,
                  const std::string &textureName = "texture",
                  bool retainPixels = false);

    // Create a texture from a image
    static Ref<Texture>
    CreateTexture(const std::string &pngFilePath,
                  const std::string &textureName = "texture",
                  bool retainPixels = false);

    inline SDL_GPUTexture *GetGPUTexture() { return m_Texture; }

    // Resizes the texture IF the provided size is different.
    // Recreates underlying texture, does not preserve any texture data
    void Resize(const glm::ivec2 &size);

    // Gets the size of the texture
    inline glm::ivec2 GetSize() { return m_Size; }

    // Sets the pixels in the texture. Assumes you are setting every pixel.
    void SetTextureData(void *pixels);

    // Sets the pixels in the region in the texture.
    // You provide the pointer to where your data begins. srcWidth is if it
    // lives in a bigger image. Does not clamp and you can cause errors!
    void SetTextureRegion(int x, int y, int w, int h, void *pixels,
                          int srcWidth = 0);

    // Binds the texture to a slot for the provided render pass
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    // override for the base class "Bindable", so that a pipeline can bind
    // either an entire material, or just a texture!
    void Bind(SDL_GPUCommandBuffer *cmbBuffer, SDL_GPURenderPass *renderPass,
              int defaultSlot = 0) override;

  public:
};
} // namespace Pyxis
