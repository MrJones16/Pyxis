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
    Renderer::SamplerType m_SamplerType = Renderer::PointWrap;
    SDL_GPUTextureCreateInfo m_TextureCreateInfo;
    SDL_GPUTexture *m_Texture;
    glm::ivec2 m_Size;

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

    // Create a texture from a image
    static Ref<Texture> CreateTexture(const std::string &pngFilePath,
                                      const std::string &textureName);

    inline SDL_GPUTexture *GetGPUTexture() { return m_Texture; }

    // Resizes the texture IF the provided size is different.
    // Recreates underlying texture, does not preserve any texture data
    void Resize(const glm::ivec2 &size);

    // Sets the pixels in the texture. Assumes you are setting every pixel.
    void SetTextureData(void *pixels);

    // Binds the texture to a slot for the provided render pass
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    // override for the base class "Bindable", so that a pipeline can bind
    // either an entire material, or just a texture!
    void Bind(SDL_GPUCommandBuffer *cmbBuffer, SDL_GPURenderPass *renderPass,
              int defaultSlot = 0) override;

  public:
};
} // namespace Pyxis
