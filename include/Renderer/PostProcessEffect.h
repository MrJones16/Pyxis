#pragma once
#include <Core/Core.h>
#include <Renderer/Material.h>
#include <Renderer/PipelineState.h>
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <string>

namespace Pyxis {

// The G-buffer targets a G-buffer post-process effect can write.
enum class GBufferTarget : int {
    Albedo = 0,     // RGBA8
    NormalUV = 1,   // RGBA16F: world normal xy (z rebuilt), uv zw
    PositionWS = 2, // RGBA32F: world position xyz, w = coverage
    Emission = 3,   // R11G11B10F
    Occlusion = 4,  // RG8: r = foreground opacity, g = wall opacity
};

// The texture format of a G-buffer target.
SDL_GPUTextureFormat GetGBufferTargetFormat(GBufferTarget target);

// A full-screen post-process pass: a fragment shader (vertex shader =
// clip space pass-through, DeferredEmissionVertex.hlsl) writing ONE target,
// plus fragment uniform data (b0, space3), an enabled flag and a name.
// The stage running it decides which textures it reads (see the stage's
// docs, e.g. DeferredRenderer::AddGBufferPostProcess) and which target it
// writes, so the same type can be reused by later stages (post lighting).
//
// Subclass it to keep typed settings and override UpdateUniform (called
// right before every draw), or use a plain PostProcessEffect and
// SetUniformData.
class PostProcessEffect {
  public:
    // `outputFormat` is the format of the target the shader writes.
    // For the G-buffer stage `gBufferOutput` names that target (its format
    // must match outputFormat; use the GBufferTarget constructor).
    PostProcessEffect(const std::string &name,
                      const std::string &fragmentShaderPath,
                      SDL_GPUTextureFormat outputFormat);
    // G-buffer stage effect writing `gBufferOutput`
    PostProcessEffect(const std::string &name,
                      const std::string &fragmentShaderPath,
                      GBufferTarget gBufferOutput);
    virtual ~PostProcessEffect() = default;

    inline const std::string &GetName() const { return m_Name; }
    inline bool IsEnabled() const { return m_Enabled; }
    inline void SetEnabled(bool enabled) { m_Enabled = enabled; }
    // false if the shader failed to compile (or was released)
    inline bool IsValid() const {
        return m_State != nullptr && m_State->IsValid();
    }
    inline const PipelineState &GetPipelineState() const { return *m_State; }
    inline SDL_GPUTextureFormat GetOutputFormat() const {
        return m_OutputFormat;
    }
    inline GBufferTarget GetGBufferOutput() const { return m_GBufferOutput; }

    // copies a plain struct as the fragment uniform (b0, space3); must match
    // the shader's cbuffer layout (16 byte multiples)
    template <typename UniformStruct>
    inline void SetUniformData(const UniformStruct &uniformStruct) {
        m_Uniform = Uniform(uniformStruct);
    }
    inline const Uniform &GetUniform() const { return m_Uniform; }

    // called by the stage right before the effect is drawn; subclasses
    // write their settings into the uniform here
    virtual void UpdateUniform() {}

    // frees the GPU objects (the renderer does this on Shutdown)
    void Release();

  protected:
    std::string m_Name;
    bool m_Enabled = true;
    SDL_GPUTextureFormat m_OutputFormat;
    GBufferTarget m_GBufferOutput = GBufferTarget::Albedo;
    std::unique_ptr<PipelineState> m_State;
    Uniform m_Uniform;
};

// G-buffer effect: bends the stored normals of foreground pixels near the
// edge of solid ground outward (from the occlusion texture), so edges
// catch light like a bevel. Writes NormalUV (uv channels kept).
// Shader: assets/shaders/GBufferEdgeNormalFragment.hlsl
class EdgeNormalEffect : public PostProcessEffect {
  public:
    EdgeNormalEffect();

    // 0 = no change, 1 = edge texels' normals point fully outward
    float m_Strength = 0.4f;
    // texels from the edge over which the effect fades out (1 = only the
    // outermost texel; max 8)
    float m_EdgeWidthTexels = 2.0f;
    // also bevel wall pixels (occlusion g) where there is no foreground
    bool m_ApplyToWalls = true;

    void UpdateUniform() override;
};

} // namespace Pyxis
