#pragma once
#include <Core/Core.h>
#include <Renderer/Pipeline.h>
#include <Renderer/Text.h>
#include <Renderer/Texture.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <vector>

namespace Pyxis {

class Renderer {
  public:
    static bool Init(const std::string &windowTitle,
                     const glm::ivec2 resolution);
    static void Shutdown();

    static void OnWindowResize(const glm::ivec2 &resolution);

    static void SetTitle(const std::string &title);
    static void SetResolution(const glm::ivec2 &resolution);
    static glm::vec2 GetResolution();

    //////////  TEXTURES  //////////
    // Textures are created from the renderer, and
    // are kept alive as long as the shared pointer
    // is alive.
    //
    // Most functions with them are done from
    // the renderer class! like binding / uploading

    // Create a basic 2d texture using a file path to a PNG
    static Ref<Texture> CreateTexture(const std::string &pngFilePath,
                                      const std::string &textureName);

    // Create a generic 2d blank texture with a set size
    static Ref<Texture> CreateTexture(const glm::ivec2 &size,
                                      const std::string &textureName);

    // Create a specific texture with advanced setup
    static Ref<Texture> CreateTexture(SDL_GPUTextureCreateInfo &textureInfo,
                                      const std::string &textureName);

    // Upload texture data to GPU if you changed it at all
    static void UploadTextureData(Ref<Texture> texture, void *pixels);

    // static void DestroyTexture(Texture &t);

    //////////  PIPELINES  //////////
    // Pipelines are the abstraction over
    // using a set of shaders, buffers,
    // textures, ect, and queue the draw calls
    // using their respective shaders.
    static int CreatePipeline(
        uint32_t maxVertices, uint32_t vertexSize,
        std::vector<SDL_GPUVertexAttribute> vertexAttributes,
        std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions,
        std::vector<SDL_GPUColorTargetInfo> colorTargetInfos,
        const std::string &vertexShaderPath,
        const std::string &fragmentShaderPath, bool targetSwapchain);

    template <typename T>
    inline static void DrawToPipeline(int pipelineIndex,
                                      std::vector<T> vertices,
                                      Ref<Material> material) {
        if (pipelineIndex >= s_Pipelines.size()) {
            PX_ERROR("Pipeline {} not found!", pipelineIndex);
            return;
        }
        PX_ASSERT(sizeof(T) == s_Pipelines[pipelineIndex]->m_VertexSize,
                  "Sizes not equal!");
        s_Pipelines[pipelineIndex]->QueueVertices(vertices, material);
    }

    static void BeginFrame();
    static void DrawPipeline(uint32_t pipelineIndex);
    static void EndFrame();

    static std::tuple<SDL_GPUTexture *, glm::ivec2> GetSwapchainTexture();
    static SDL_GPUTextureFormat GetSwapchainTextureFormat();

    // Text rendering API
    static int LoadFont(const std::string &fontPath, uint32_t fontSize);
    static void UnloadFont(int fontID);
    static uint32_t QueueText(int fontID, const std::string &text,
                              const glm::vec2 &position,
                              const glm::vec4 &color);
    static glm::ivec2 GetTextSize(int fontID, const std::string &text);

  private:
    // helper function to bind texture. Should be done internally from pipelines
    static void BindTexture(SDL_GPURenderPass *renderPass,
                            Ref<Texture> &texture, int slot = 0);

  protected:
    static SDL_Window *s_Window;
    static SDL_GPUDevice *s_GPUDevice;
    static SDL_GPUCommandBuffer *s_GPUCommandBuffer;

    static std::vector<Pipeline *> s_Pipelines;

    static std::vector<Texture *> s_Textures;

    // maybe on these
    static glm::ivec2 s_RenderResolution;
    static float s_RenderPadding;
};

} // namespace Pyxis
