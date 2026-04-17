#pragma once
#include <Core/Core.h>
#include <Renderer/Pipeline.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <vector>

namespace Pyxis {

class Renderer {
  public:
    static bool Init(const std::string &windowTitle,
                     const glm::ivec2 &resolution);
    static void Shutdown();

    static void OnWindowResize(const glm::ivec2 &resolution);

    static void SetTitle(const std::string &title);
    static void SetResolution(const glm::ivec2 &resolution);
    static glm::vec2 GetResolution();

    static int CreatePipeline(
        uint32_t maxVertices, uint32_t vertexSize,
        std::vector<SDL_GPUVertexAttribute> vertexAttributes,
        std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
        std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
        const std::string &vertexShaderPath,
        const std::string &fragmentShaderPath, bool targetSwapchain);

    template <typename T>
    inline static void DrawToPipeline(int pipelineIndex,
                                      std::vector<T> vertices) {
        if (pipelineIndex >= s_Pipelines.size()) {
            PX_ERROR("Pipeline {} not found!", pipelineIndex);
            return;
        }
        s_Pipelines[pipelineIndex]->QueueVertices(vertices);
    }

    static void BeginFrame();
    static void DrawPipeline(uint32_t pipelineIndex);
    static void EndFrame();

    static std::tuple<SDL_GPUTexture *, glm::ivec2> GetSwapchainTexture();
    static SDL_GPUTextureFormat GetSwapchainTextureFormat();

  protected:
    static SDL_Window *s_Window;
    static SDL_GPUDevice *s_GPUDevice;
    static SDL_GPUCommandBuffer *s_GPUCommandBuffer;

    static std::vector<Pipeline *> s_Pipelines;

    // maybe on these
    static glm::ivec2 s_RenderResolution;
    static float s_RenderPadding;
};

} // namespace Pyxis
