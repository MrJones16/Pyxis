#pragma once

#include "Renderer/Material.h"
#include <Renderer/Pipeline.h>
#include <Renderer/RenderBatch.h>
namespace Pyxis {
// RenderPass is the object defining the output targets of a particular pass in
// the overall rendering pipeline
//
// There can be multiple pipelines that draw into the same pass
//
// Pipelines are drawn into the pass with the data from RenderBatch
class RenderPass {
    struct RenderPassDescription {};

    // Set the vertex uniform data that this pipeline uses
    void SetVertexUniform(Ref<Uniform> uniform);
    inline Ref<Uniform> GetVertexUniform() { return m_VertexUniform; }

    // Set the Fragment uniform data that this pipeline uses
    void SetFragmentUniform(Ref<Uniform> uniform);
    inline Ref<Uniform> GetFragmentUniform() { return m_FragmentUniform; }

    // Set the resolution this pass renders at. Resizes the output textures as
    // well!
    void SetResolution(const glm::ivec2 &resolution);
    // Get the resolution this pipeline renders at
    inline glm::ivec2 GetResolution() { return m_Resolution; }

    void BeginPass();
    // Records a draw of every mesh queued in `batch` with `state`. The
    // uniform data (bound to slot 0 of the vertex / fragment stage) is
    // copied now. The batch must stay alive and unchanged until EndPass.
    void Draw(const Pipeline &pipeline, RenderBatch &batch,
              const Uniform *vertexUniform = nullptr,
              const Uniform *fragmentUniform = nullptr);
    void EndPass();

  private:
    // output color targets
    std::vector<SDL_GPUColorTargetInfo> m_ColorTargetInfos;
    // whether or not we target screen output
    bool m_TargetSwapchain = false;

    // only applicable if you are not targeting swapchain.
    glm::ivec2 m_Resolution = {1920, 1080};

    // Depth & Stencil
    bool m_HasDepthStencilTexture = false;
    SDL_GPUDepthStencilTargetInfo m_DepthStencilTargetInfo{};

    // two uniforms kept alive and used by this pass
    Ref<Uniform> m_VertexUniform = nullptr, m_FragmentUniform = nullptr;
};
} // namespace Pyxis
