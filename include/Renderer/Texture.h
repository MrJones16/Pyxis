#pragma once
#include <Renderer/Renderer.h>

namespace Pyxis {

enum SamplerType {
    PointClamp,
    PointWrap,
    LinearClamp,
    LinearWrap,
    // AnisotropicClamp,
    // AnisotropicWrap
};

class Texture {
  protected:
    static std::map<SamplerType, SDL_GPUSampler *> s_Samplers;

    SamplerType m_SamplerType = PointWrap;
    SDL_GPUTextureCreateInfo m_TextureCreateInfo;
    SDL_GPUTexture *m_Texture;
    glm::ivec2 m_Size;

  public:
    static bool Init(SDL_GPUDevice *device);
    static void Shutdown(SDL_GPUDevice *device);

    // Create a generic 2d blank texture with a set size
    Texture(SDL_GPUDevice *device, const glm::ivec2 &size,
            const std::string &textureName);

    // Create a specific texture with advanced setup
    Texture(SDL_GPUDevice *device, SDL_GPUTextureCreateInfo &textureInfo,
            const std::string &textureName);

    ~Texture();

    inline SDL_GPUTexture *GetGPUTexture() { return m_Texture; }

    void Resize(const glm::ivec2 &size);

    void SetTextureData(void *pixels);
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    friend class Renderer;
    friend class Pipeline;

  public:
};
} // namespace Pyxis
