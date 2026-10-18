#pragma once
#include <Core/Core.h>
#include <SDL3/SDL_error.h>
#include <Renderer/RenderPass.h>
#include <Renderer/UploadQueue.h>
#include <SDL3/SDL_gpu.h>
#include <memory>
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

class PipelineState;
class BatchBuffer;
struct Uniform;

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
        glm::ivec2 SwapchainSize = {0, 0};
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

    // Try to begin a frame: acquires the frame's command buffer and the
    // swapchain texture. Returns false when there is no swapchain texture
    // this frame (e.g. minimized window): skip rendering this frame and
    // don't call EndFrame (queued uploads are still submitted).
    static bool BeginFrame();
    // Ends the current frame: records any pending uploads and submits the
    // frame's command buffer. Does nothing if no frame was begun.
    static void EndFrame();
    // True between a successful BeginFrame and EndFrame.
    static bool IsFrameActive();
    // The current frame's command buffer / swapchain texture (only valid
    // while IsFrameActive()).
    static FrameData &GetFrameData();
    static SDL_GPUTextureFormat GetSwapchainTextureFormat();

    // The per-frame upload queue: texture / buffer uploads queued here are
    // recorded in one copy pass on the frame's command buffer before the
    // next render pass (or at EndFrame), instead of one submit per upload.
    static UploadQueue &GetUploadQueue();
    // Records the pending uploads now: on the frame's command buffer when a
    // frame is active, otherwise on a separate command buffer that is
    // submitted right away. Never call this inside a render pass.
    static void FlushPendingUploads();

    ////////////////////
    /// RENDER PASSES ///
    ////////////////////
    // A frame draws in render passes:
    //
    //   Renderer::BeginPass(passDescription);       // targets
    //   Renderer::Draw(pipelineStateA, batchA, &vertexUniform);
    //   Renderer::Draw(pipelineStateB, batchB);     // any number of states
    //   Renderer::EndPass();
    //
    // Draw only records what to draw. EndPass then prepares every drawn
    // batch (ordering, merging, queuing its vertex/index upload), records
    // ALL pending uploads (batches + queued texture uploads) in one copy
    // pass, begins the GPU render pass on the description's attachments
    // (viewport = attachment size), and draws each recorded (state, batch)
    // in Draw order. Each state's color targets / depth state must match
    // the pass attachments (mismatches are logged and skipped).
    //
    // Returns false (and the pass is ignored) when no frame is active.
    static bool BeginPass(const RenderPassDescription &description);
    // Records a draw of every mesh queued in `batch` with `state`. The
    // uniform data (bound to slot 0 of the vertex / fragment stage) is
    // copied now. The batch must stay alive and unchanged until EndPass.
    static void Draw(const PipelineState &state, BatchBuffer &batch,
                     const Uniform *vertexUniform = nullptr,
                     const Uniform *fragmentUniform = nullptr);
    static void EndPass();

  private:
    ////////////////////////////////////////////////////////
    /// PRIVATE FUNCTIONS TO BE USED BY TEXTURE/PIPELINE ///
    ////////////////////////////////////////////////////////
    /// or until i want them outside of that... lol

    friend class Texture;
    friend class PipelineState;
    friend class BatchBuffer;

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
    static std::unique_ptr<UploadQueue> s_UploadQueue;

    // the pass being recorded (BeginPass .. EndPass)
    struct RecordedDraw {
        const PipelineState *state = nullptr;
        BatchBuffer *batch = nullptr;
        std::vector<uint8_t> vertexUniform;
        std::vector<uint8_t> fragmentUniform;
    };
    static bool s_PassOpen;
    static RenderPassDescription s_PassDescription;
    static std::vector<RecordedDraw> s_PassDraws;
};

} // namespace Pyxis
