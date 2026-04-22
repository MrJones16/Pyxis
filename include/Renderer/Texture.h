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
    static std::map<SamplerType, SDL_GPUSampler *> s_Samplers;

    SamplerType m_SamplerType = PointWrap;
    SDL_GPUTexture *m_Texture;
    glm::ivec2 m_Size;
    SDL_GPUDevice *m_Device;

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

    void SetTextureData(SDL_GPUDevice *device,
                        SDL_GPUCommandBuffer *commandBuffer, void *pixels);
    void Bind(SDL_GPURenderPass *renderPass, uint8_t slot = 0);

    friend class Renderer;

  public:
};
} // namespace Pyxis
