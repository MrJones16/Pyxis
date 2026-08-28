#pragma once
#include <Core/Core.h>
#include <Renderer/Text.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <vector>

namespace Pyxis {
// order of vertices going forward for consistency
// 0 1
// 2 3
// tl, tr, bl, br

static const std::vector<glm::vec3> QuadVertices{
    {-0.5f, 0.5f, 0.0f},  // tl
    {0.5f, 0.5f, 0.0f},   // tr
    {-0.5f, -0.5f, 0.0f}, // bl
    {0.5f, -0.5f, 0.0f}   // br
};
static const std::vector<uint32_t> QuadIndices{0, 2, 3, 3, 1, 0};

class Renderer {
    friend class Application;

  public:
    static bool Init(const std::string &windowTitle,
                     const glm::ivec2 resolution, bool debug = false);
    static void Shutdown();

    //////////////////////
    /// EVENT HANDLING ///
    //////////////////////

    // Called by the application when there is a window resize
    // static void OnWindowResize(const glm::ivec2 &resolution);

    /////////////////////
    /// GETS AND SETS ///
    /////////////////////
    static void SetTitle(const std::string &title);
    static void SetResolution(const glm::ivec2 &resolution);

    static glm::vec2 GetResolution();

    //////////////////////
    /// MAIN FUNCTIONS ///
    //////////////////////

    // Try to begin frame and grab the swapchain texture.
    static bool BeginFrame();

    // End the current frame
    static void EndFrame();

    // Text rendering API
    static int LoadFont(const std::string &fontPath, uint32_t fontSize);
    static void UnloadFont(int fontID);
    static glm::ivec2 GetTextSize(int fontID, const std::string &text);

  private:
    ////////////////////////////////////////////////////////
    /// PRIVATE FUNCTIONS TO BE USED BY TEXTURE/PIPELINE ///
    ////////////////////////////////////////////////////////

    friend class Texture;
    friend class Pipeline;
    static inline SDL_Window *GetWindow() { return s_Window; };
    static inline SDL_GPUDevice *GetGPUDevice() { return s_GPUDevice; };
    static inline SDL_GPUCommandBuffer *BeginGPUCommandBuffer() {
        return SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    };
    static void inline EndGPUCommandBuffer(
        SDL_GPUCommandBuffer *commandBuffer) {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
    };
    static std::pair<SDL_GPUTexture *, glm::ivec2> GetSwapchainTexture();
    static SDL_GPUTextureFormat GetSwapchainTextureFormat();

  protected:
    static SDL_Window *s_Window;
    static SDL_GPUDevice *s_GPUDevice;
    static SDL_GPUCommandBuffer *s_GPUCommandBuffer;
    static SDL_GPUTexture *s_SwapchainTexture;
    static glm::ivec2 s_SwapchainSize;
};

} // namespace Pyxis
