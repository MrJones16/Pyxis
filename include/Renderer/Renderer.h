#pragma once
#include <Core/Core.h>
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
    enum SamplerType {
        PointClamp,
        PointWrap,
        LinearClamp,
        LinearWrap,
        // AnisotropicClamp,
        // AnisotropicWrap
    };

    struct FrameData {
        bool AcquiredSwapchain = false;
        SDL_GPUCommandBuffer *GPUCommandBuffer = nullptr;
        SDL_GPUTexture *SwapchainTexture = nullptr;
        glm::ivec2 SwapchainSize;
    };

    static std::map<SamplerType, SDL_GPUSampler *> s_Samplers;

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

    // Sets the title of the main window
    static void SetTitle(const std::string &title);
    // sets the resolution of the main window
    static void SetResolution(const glm::ivec2 &resolution);
    static glm::vec2 GetResolution();

    //////////////////////
    /// MAIN FUNCTIONS ///
    //////////////////////

    // Try to begin frame and grab the swapchain texture.
    // The FrameData property AcquiredSwapchain is the bool for a successful
    // frame beginning!
    static FrameData &BeginFrame();
    // End the current frame
    static void EndFrame();
    static SDL_GPUTextureFormat GetSwapchainTextureFormat();

  private:
    ////////////////////////////////////////////////////////
    /// PRIVATE FUNCTIONS TO BE USED BY TEXTURE/PIPELINE ///
    ////////////////////////////////////////////////////////
    /// or until i want them outside of that... lol

    friend class Texture;
    friend class Pipeline;

    // Get the main window. we only support 1...
    static SDL_Window *GetWindow();
    // Get the current GPU Device. Changing during runtime would
    // be very challenging
    static SDL_GPUDevice *GetGPUDevice();
    // Acquire a command buffer
    static SDL_GPUCommandBuffer *BeginGPUCommandBuffer();
    // Submit the command buffer to the gpu
    static void EndGPUCommandBuffer(SDL_GPUCommandBuffer *commandBuffer);

  protected:
    static FrameData s_FrameData;
    static SDL_Window *s_Window;
    static SDL_GPUDevice *s_GPUDevice;
};

} // namespace Pyxis
