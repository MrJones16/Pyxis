#pragma once
#include <Core/Core.h>

namespace Pyxis {

// struct RenderData {
//     SDL_GPUDevice *device = nullptr;
// };

class Renderer {
  public:
    static bool Init(const std::string &windowTitle,
                     const glm::ivec2 &resolution);
    static void Shutdown();

    static void SetTitle(const std::string &title);
    static void SetResolution(const glm::ivec2 &resolution);

    static void BeginFrame();
    static void EndFrame();

  protected:
    static glm::ivec2 s_Resolution;
    static glm::ivec2 s_RenderResolution;
    static float s_RenderPadding;
};

} // namespace Pyxis
