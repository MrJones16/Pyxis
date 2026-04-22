#pragma once
#include <Core/Core.h>
#include <SDL3/SDL_gpu.h>

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
    SamplerType m_SamplerType = PointWrap;
    SDL_GPUTexture *m_Texture;
    glm::ivec2 m_Size;
    SDL_GPUDevice *m_Device;

  private:
    // Create a generic 2d blank texture with a set size
    Texture(SDL_GPUDevice *device, const glm::ivec2 &size,
            const std::string &textureName);

    // Create a specific texture with advanced setup
    Texture(SDL_GPUDevice *device, SDL_GPUTextureCreateInfo &textureInfo,
            const std::string &textureName);

    ~Texture();
    friend class Renderer;

  public:
    void SetData();
    void Bind(int slot = 0);
};
} // namespace Pyxis
