#pragma once
#include <Components/CameraComponent.h>
#include <Renderer/DefaultRenderer.h>
#include <memory>
#include <unordered_map>

namespace Pyxis {

// A shader used in the deferred G-pass: a PipelineState writing the four
// G-buffer targets (albedo color, normal/uv, position, emission) with
// depth, plus the batch of quads queued for it this frame.
// Create one with DeferredRenderer::CreateGPassShader and pass it to
// DeferredRenderer::DrawQuad. See CreateGPassShader for the shader contract.
class DeferredGPassShader {
  public:
    DeferredGPassShader(const std::string &name,
                        std::unique_ptr<PipelineState> state,
                        std::unique_ptr<BatchBuffer> batch);
    inline const std::string &GetName() const { return m_Name; }
    // false if the shaders failed to compile or it was released
    inline bool IsValid() const {
        return m_State != nullptr && m_State->IsValid();
    }
    inline PipelineState &GetPipelineState() { return *m_State; }
    inline BatchBuffer &GetBatch() { return *m_Batch; }
    // frees the GPU objects now (DeferredRenderer::Shutdown does this for
    // every G-pass shader that is still alive)
    void Release();

  private:
    std::string m_Name;
    std::unique_ptr<PipelineState> m_State;
    std::unique_ptr<BatchBuffer> m_Batch;
};

// Options for DeferredRenderer::CreateGPassShader.
struct DeferredGPassShaderOptions {
    // max quads queued per frame with this shader
    int maxQuads = 10000;
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    // blending of the albedo color target (normal/uv, position and emission
    // targets are never blended)
    SDL_GPUColorTargetBlendState albedoBlend = BlendStates::Alpha();
};

// Draws the world through a G-buffer with a camera (SetViewProjectionMatrix)
// at its own (low) render resolution, lights it, and composites the lit
// image to the screen through the DefaultRenderer.
// In SDL3, the depth is from 0 (near) to 1 (far), and otherwise -1 to 1 on x,y
class DeferredRenderer {
  public:
    // vertex format of every G-pass shader (DrawQuad writes these)
    // (no vertex normals: surface normals come from the normal map, slot 1,
    // turned into world space with the quad's tangent / bitangent)
    struct DeferredTextureVertex {
        glm::vec4 color; // tint
        glm::vec2 uv;
        // world space directions of the textures' +x (tangent.xy) and +y
        // (bitangent.zw): the quad's rotation and flip
        glm::vec4 tangentBitangent;
        glm::vec3 position; // world position
    };

  private:
    static glm::ivec2 s_RenderResolution;
    // background color of the scene (G-buffer albedo clear color)
    static glm::vec4 s_ClearColor;

    // G-pass shaders: the default one (DeferredG*.hlsl) and every shader
    // made with CreateGPassShader, drawn in creation order
    static Ref<DeferredGPassShader> s_DefaultGPassShader;
    static std::vector<std::weak_ptr<DeferredGPassShader>> s_GPassShaders;
    static int s_DefaultMaxQuads;

    // lights additively into the lighting texture, reading the G-buffer
    static std::unique_ptr<PipelineState> s_LightingState;
    static std::unique_ptr<BatchBuffer> s_LightingBatch;
    // unlit debug lines on top of the lit image (line list), same pass
    static std::unique_ptr<PipelineState> s_LineState;
    static std::unique_ptr<BatchBuffer> s_LineBatch;

    // camera view projection, vertex uniform slot 0 of every state above
    static Ref<Uniform> s_CameraUniform;

    static Ref<Material> s_GBufferMaterial;
    static Ref<Texture> s_GTextureColor;
    static Ref<Texture> s_GTexturePositionNS;
    static Ref<Texture> s_GTextureNormalUV;
    // emission multiplier on albedo (R16 float)
    static Ref<Texture> s_GTextureEmission;

    // 1x1 maps bound to the G-pass material slots a quad doesn't provide:
    // flat normal (0.5, 0.5, 1.0, strength 0) and emission 0 (R16 float)
    static Ref<Texture> s_DefaultNormalTexture;
    static Ref<Texture> s_DefaultEmissionTexture;
    // G-pass materials made for plain textures (albedo + default maps),
    // keyed by the texture; dropped once only the material holds it
    static std::unordered_map<Texture *, Ref<Material>> s_TextureGPassMaterials;

    // adds albedo * emission into the lighting texture once per frame
    // (full-screen quad, before the lights)
    static std::unique_ptr<PipelineState> s_EmissionState;
    static std::unique_ptr<BatchBuffer> s_EmissionBatch;
    static Ref<Material> s_EmissionMaterial;

    static Ref<Texture> s_LightingTexture;
    static Ref<Material> s_LightingTextureMaterial;

    static Ref<Texture> s_DepthTexture;

    struct DeferredLineVertex {
        glm::vec4 color;
        glm::vec4 position; // world position (xyz), w unused
    };

    struct DeferredLightVertex {
        glm::vec4 color;
        glm::vec4 position;       // quad corner, world space
        glm::vec4 positionCenter; // light center, world space
        glm::vec4 rad_intensity_falloff_type;
    };

  public:
    static bool Init(int maxQuads = 10000,
                     const glm::ivec2 resolution = {480, 270});
    static void Shutdown();

    // Sets the background (clear) color of the scene, RGBA in 0..1.
    // Empty / transparent areas show this color. It goes through lighting
    // like any other albedo, so with the full-screen white diffuse light it
    // appears as-is. Can be called before or after Init.
    static void SetClearColor(const glm::vec4 &color);
    static glm::vec4 GetClearColor();

    // call this when the screen resizes to update depth texture size
    static void OnWindowResize(const glm::ivec2 &resolution);

    // different for this renderer, as it sets the render res which is lower
    // usually
    static void Resize(const glm::ivec2 &resolution);

    // camera stuff
    struct CameraUniform {
        glm::mat4 ViewProjectionMatrix;
    };
    static void SetViewProjectionMatrix(glm::mat4 &ViewProjectionMatrix);

    ///////////////////////
    /// G-PASS SHADERS ///
    ///////////////////////
    // Creates a custom G-pass shader. Quads drawn with it (DrawQuad's
    // `gPassShader`) go through the same G-pass as everything else and are
    // lit like any other pixel.
    //
    // Shader contract (see assets/shaders/DeferredGVertex.hlsl and
    // DeferredGFragment.hlsl, the default pair, as a starting point):
    // - vertex input (DeferredTextureVertex): COLOR0 float4 tint (location
    //   0), TEXCOORD0 float2 uv (1), TEXCOORD1 float4 world tangent.xy /
    //   bitangent.zw (2), POSITION0 float3 world position (3)
    // - vertex uniform: cbuffer at register(b0, space1) with
    //   float4x4 viewProj (the camera)
    // - fragment outputs: SV_Target0 albedo (RGBA8), SV_Target1 world
    //   normal.xy / uv.zw (RGBA16F; z is rebuilt by the lighting), SV_Target2
    //   world position xyz, w unused (RGBA32F), SV_Target3 emission (R16F,
    //   multiplier on albedo, added once by the lighting pass)
    // - material slots (t/s 0-2, space2): 0 albedo, 1 normal map (RGBA8
    //   tangent space: rgb = normal * 0.5 + 0.5, a = normal strength, i.e.
    //   the blend from flat (0, 0, 1) to the map normal), 2 emission map
    //   (R16 float). DrawQuad binds 1x1 defaults to slots a quad lacks
    //   (white albedo, flat normal, emission 0), so all three always exist.
    //   A plain Texture is the albedo; a Material binds its textures to
    //   their slots and pushes its uniform data (Material::SetUniformData)
    //   to fragment uniform slots (register(bN, space3)).
    // - discard fully transparent pixels (as the default shader does) so
    //   they don't write depth / normal / position.
    // The DeferredRenderer keeps a weak reference: the shader is drawn for
    // as long as you hold the Ref. Shutdown releases the GPU objects of the
    // ones still alive.
    static Ref<DeferredGPassShader>
    CreateGPassShader(const std::string &name,
                      const std::string &vertexShaderPath,
                      const std::string &fragmentShaderPath,
                      const DeferredGPassShaderOptions &options = {});
    // the built-in G-pass shader used when DrawQuad gets no shader
    static Ref<DeferredGPassShader> GetDefaultGPassShader();
    // the 1x1 maps used for missing G-pass material slots (see above)
    static Ref<Texture> GetDefaultNormalTexture();
    static Ref<Texture> GetDefaultEmissionTexture();

    // G-pass: draws every quad queued with DrawQuad / DrawText into the
    // G-buffer (one render pass, each G-pass shader in creation order).
    static void DrawObjects();
    // Lighting pass: draws the lights, then the queued debug lines on top
    // of the lit image (one render pass).
    static void DrawLights();

    ////////////////////
    /// DEBUG LINES ///
    ////////////////////
    // Lines are queued in world coordinates and drawn UNLIT, after lighting,
    // straight into the low-res lit image (1 render pixel wide), so they
    // follow the camera and always appear over the world (sand sim included).
    // They are flushed by DrawLights; queue them before calling it.
    // `z` is kept for API consistency (0 = front, 1 = back) but lines are
    // drawn without depth testing.
    static void DrawLine(const glm::vec3 &start, const glm::vec3 &end,
                         const glm::vec4 &color = {1, 1, 1, 1});
    static void DrawLine(const glm::vec2 &start, const glm::vec2 &end,
                         const glm::vec4 &color = {1, 1, 1, 1}, float z = 0);
    // Polyline through `points`; `closed` connects the last point to the
    // first (e.g. polygon outlines).
    static void DrawLines(const std::vector<glm::vec2> &points,
                          const glm::vec4 &color = {1, 1, 1, 1},
                          bool closed = false, float z = 0);

    // if useMainCamera is set, it will assume the camera size matches the
    // window size, and use the offset
    static void DrawToScreen(float depth = 1, bool useMainCamera = false);
    static void Debug_DrawColorToScreen();
    static void Debug_DrawNormalUVToScreen();
    static void Debug_DrawPositionToScreen();

    /// uv bounds are xmin, ymin, xmax, ymax. `bindable` is a Texture (the
    /// albedo) or a Material (slots 0-2, see CreateGPassShader); null = white
    /// texture. Missing slots get the default maps. Unrotated: the normal
    /// map's tangent is +x, bitangent +y. `gPassShader` null = the default
    /// G-pass shader.
    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Bindable> bindable = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const Ref<DeferredGPassShader> &gPassShader = nullptr);
    /// A quad of `size` centered on the origin, moved / rotated / scaled by
    /// `transform` (world space). The normal map's tangent / bitangent are
    /// the transform's x / y axes (so they follow rotation and flips; scale
    /// is ignored for them). Null material = white texture.
    static void DrawQuad(const glm::mat4 &transform,
                         const glm::vec2 &size = {1, 1},
                         Ref<Material> material = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const Ref<DeferredGPassShader> &gPassShader = nullptr);
    static void DrawText(Ref<Font> font, glm::vec3 position,
                         const std::string &text,
                         const glm::vec4 &color = {1, 1, 1, 1},
                         const glm::vec2 scale = {1, 1});
    enum LightType { Point, Diffuse };
    static void DrawLight(const glm::vec3 &position, const glm::vec4 &color,
                          float radius, float intensity, float falloff,
                          float type);

  private:
    // couple of helpers to separate the code
    static void CreateLightingState(int maxQuads);
    static void CreateLineState(int maxLines);
    static void CreateEmissionState();
    static void CreateDefaultMaps();
    // the bindable a G-pass quad is queued with: every material slot (0-2)
    // filled, defaults where missing
    static Ref<Bindable> GetGPassBindable(const Ref<Bindable> &bindable);
};
} // namespace Pyxis
