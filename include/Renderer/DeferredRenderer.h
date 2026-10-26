#pragma once
#include <Components/CameraComponent.h>
#include <Components/LightComponent.h>
#include <Components/ParallaxLayerComponent.h>
#include <Renderer/PostProcessEffect.h>
#include <Renderer/DefaultRenderer.h>
#include <memory>
#include <unordered_map>

namespace Pyxis {

// A shader used in the deferred G-pass: a PipelineState writing the five
// G-buffer targets (albedo color, normal/uv, position, emission, occlusion) with
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
    // channels of the RG8 occlusion target this shader writes (R =
    // foreground opacity, G = wall opacity), so shaders writing different
    // channels don't clobber each other
    SDL_GPUColorComponentFlags occlusionWriteMask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G;
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

    // Texels of padding on EACH side of the internal render targets (in
    // the low-res render texel space, 1 texel = 1 element cell). The
    // G-buffer, jump flood, distance and lighting targets are all
    // (render resolution + 2 * RenderPadding) big and the G-pass camera
    // covers that padded area, so off-screen occluders / sky can cast light
    // and shadow into the view. DrawToScreen crops the padding away, so
    // the visible image is unchanged.
    static constexpr int RenderPadding = 128;

  private:
    // the visible (un-padded) render resolution, as given to Init / Resize
    static glm::ivec2 s_RenderResolution;
    // s_RenderResolution + 2 * RenderPadding: size of every internal target
    static glm::ivec2 s_PaddedResolution;
    // world position at the center of the view (from the inverse view
    // projection), where Directional light quads are centered
    static glm::vec3 s_ViewCenterWorld;
    // world units per render texel (from the view projection), x and y
    static glm::vec2 s_WorldPerTexel;
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
    static Ref<Texture> s_GTexturePositionWS;
    static Ref<Texture> s_GTextureNormalUV;
    // emitted light, RGB (R11G11B10 float, may exceed 1), independent of
    // the albedo
    static Ref<Texture> s_GTextureEmission;
    // occlusion texture which is for blocking light
    // r8g8, where r is foreground opacity, g is background opacity
    static Ref<Texture> s_GTextureOcclusion;

    // 1x1 maps bound to the G-pass material slots a quad doesn't provide:
    // flat normal (0.5, 0.5, 1.0, strength 0) and emission 0 (RGBA8 black)
    static Ref<Texture> s_DefaultNormalTexture;
    static Ref<Texture> s_DefaultEmissionTexture;
    // G-pass materials made for plain textures (albedo + default maps),
    // keyed by the texture; dropped once only the material holds it
    static std::unordered_map<Texture *, Ref<Material>> s_TextureGPassMaterials;

    // adds the emission rgb into the lighting texture once per frame
    // (full-screen quad, before the lights)
    static std::unique_ptr<PipelineState> s_EmissionState;
    static std::unique_ptr<BatchBuffer> s_EmissionBatch;
    static Ref<Material> s_EmissionMaterial;

    static Ref<Texture> s_LightingTexture;
    static Ref<Material> s_LightingTextureMaterial;

    //////////////////////
    /// JUMP FLOODING ///
    //////////////////////
    // Two ping-pong RGBA16F targets (padded size). Each texel holds the
    // texel coordinate of the nearest seed found so far for two fields at
    // once: rg = nearest FOREGROUND occluder (occlusion.r > 0), ba =
    // nearest OPEN SKY pixel (occlusion.r == 0 and occlusion.g == 0, no
    // foreground and no wall). (-1, -1) = no seed found yet.
    static Ref<Texture> s_JumpFloodTextures[2];
    // the seed pass reads the occlusion texture, writes s_JumpFloodTextures[0]
    static std::unique_ptr<PipelineState> s_JumpFloodSeedState;
    static Ref<Material> s_JumpFloodSeedMaterial;
    // each flood pass reads one ping-pong texture and writes the other,
    // with the step size in a fragment uniform
    static std::unique_ptr<PipelineState> s_JumpFloodStepState;
    // s_JumpFloodStepMaterials[i] reads s_JumpFloodTextures[i]
    static Ref<Material> s_JumpFloodStepMaterials[2];
    // RG16F (padded size): r = distance in texels to the nearest foreground
    // occluder, g = distance to the nearest open sky pixel
    // (DistanceFieldNone when there is none). Read by the lighting.
    static Ref<Texture> s_DistanceFieldTexture;
    static std::unique_ptr<PipelineState> s_DistanceFieldState;
    // s_DistanceFieldMaterials[i] reads s_JumpFloodTextures[i] (the last
    // flood pass's output decides which one is used)
    static Ref<Material> s_DistanceFieldMaterials[2];
    // one full-screen quad (clip space), queued once per material that a
    // full-screen pass reads; the batches persist between frames
    static std::unique_ptr<BatchBuffer> s_JumpFloodSeedBatch;
    static std::unique_ptr<BatchBuffer> s_JumpFloodStepBatches[2];
    static std::unique_ptr<BatchBuffer> s_DistanceFieldBatches[2];

    // debug view: replaces the lit image with the distance field
    static bool s_DebugShowDistanceField;
    static std::unique_ptr<PipelineState> s_DistanceFieldDebugState;
    static std::unique_ptr<BatchBuffer> s_DistanceFieldDebugBatch;
    static Ref<Material> s_DistanceFieldDebugMaterial;

    // lighting settings (fragment uniform slot 0 of the lighting shader)
    struct LightingSettings {
        // texels of solid that light partly soaks through (see
        // SetLightBleedDistance)
        float bleedDistance;
        // max texels a Directional light marches looking for open sky
        float sunMaxDistance;
        // safety cap on march iterations per light per pixel
        float maxMarchSteps;
        // bleed falloff shape: 0 = linear, 1 = rescaled exponential
        float bleedCurve;
        // k of the exponential falloff
        float bleedSharpness;
        // keeps the struct a multiple of 16 bytes (matches the HLSL cbuffer)
        float padding[3];
    };
    static_assert(sizeof(LightingSettings) == 8 * sizeof(float));
    static LightingSettings s_LightingSettings;

    static Ref<Texture> s_DepthTexture;

    ///////////////////////
    /// PARALLAX LAYERS ///
    ///////////////////////
    // layers queued this frame by DrawParallaxLayer (drawn and cleared by
    // DrawLights, back to front)
    struct QueuedParallaxLayer {
        Ref<Texture> texture;
        glm::vec2 parallaxFactor;
        glm::vec4 tint;
        glm::vec2 offset;
        glm::vec2 tileWorldSize;
        bool repeatX, repeatY;
        float depth;
        float sunLitFactor;
    };
    static std::vector<QueuedParallaxLayer> s_ParallaxLayers;
    // textures loaded from ParallaxLayerComponent::m_TexturePath
    static std::unordered_map<std::string, Ref<Texture>> s_ParallaxTextures;
    static std::unique_ptr<PipelineState> s_ParallaxState;
    // one full-screen quad per layer, each with its own material (layer
    // texture + PositionWS + the layer's uniform), reused between frames
    static std::unique_ptr<BatchBuffer> s_ParallaxBatch;
    static std::vector<Ref<Material>> s_ParallaxMaterials;
    // colour layers with m_SunLitFactor are multiplied by
    static glm::vec4 s_ParallaxSunColor;

    ////////////////////////////////
    /// G-BUFFER POST PROCESSING ///
    ////////////////////////////////
    // run in order after the G-pass, before the distance field
    static std::vector<Ref<PostProcessEffect>> s_GBufferPostProcesses;
    // G-buffer textures as inputs (t0 albedo, t1 normal/uv, t2 position,
    // t3 emission, t4 occlusion) and its full-screen quad
    static Ref<Material> s_GBufferPostProcessMaterial;
    static std::unique_ptr<BatchBuffer> s_GBufferPostProcessBatch;
    // scratch (ping-pong) copy per G-buffer target, made on first use
    static Ref<Texture> s_GBufferScratchTextures[5];

    struct DeferredLineVertex {
        glm::vec4 color;
        glm::vec4 position; // world position (xyz), w unused
    };

    struct DeferredLightVertex {
        glm::vec4 color;
        glm::vec4 position;       // quad corner, world space
        glm::vec4 positionCenter; // light center, world space
        glm::vec4 rad_intensity_falloff_type;
        // Point: spotlight direction xy (world, normalized), cos(inner half
        // angle), cos(outer half angle) (both -1 = unlimited).
        // Directional: direction the light travels xy (world, normalized).
        glm::vec4 directionCone;
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
    // Sets the camera of the G-pass, lights and lines. The matrix should
    // cover the visible view; it is scaled so it covers the padded targets
    // (RenderPadding texels more on each side, orthographic cameras).
    static void SetViewProjectionMatrix(glm::mat4 &ViewProjectionMatrix);

    // the visible render resolution / the padded size of internal targets
    static glm::ivec2 GetRenderResolution();
    static glm::ivec2 GetPaddedResolution();

    ////////////////////////
    /// LIGHTING SETTINGS ///
    ////////////////////////
    // How many texels deep light soaks into (and through) solids. The
    // first `texels` occluding texels a shadow ray crosses only partly
    // attenuate it (ramping up to full strength), so lit surfaces glow a
    // few texels deep and very thin solids leak a little light. 0 = hard,
    // exact shadows. Default 3.
    static void SetLightBleedDistance(float texels);
    static float GetLightBleedDistance();
    // Shape of the bleed falloff over [0, bleedDistance]: 0 = linear (hard
    // kink where it reaches 0), 1 = exponential rescaled to hit exactly 0 at
    // bleedDistance (fast initial drop, soft tail), in between a blend.
    // Clamped to [0, 1]. Default 0.5.
    static void SetLightBleedCurve(float curve);
    static float GetLightBleedCurve();
    // k in exp(-k * depth / bleedDistance) for the exponential part of the
    // falloff; higher = more light lost in the first texels. >= 0, 0 acts
    // as linear. Default 4.
    static void SetLightBleedSharpness(float k);
    static float GetLightBleedSharpness();
    // How far (texels) a Directional (sun) light marches from each pixel
    // looking for open sky. Pixels farther than this from any open sky
    // (along the sun direction) are in shadow. Keep it <= RenderPadding so
    // rays don't leave the padded targets. Default 96.
    static void SetSunMaxDistance(float texels);
    static float GetSunMaxDistance();

    // Debug: show the distance field instead of the lit image (red =
    // distance to the nearest foreground occluder, green = distance to the
    // nearest open sky, with banding every 16 texels; seeds are black).
    static void SetDebugShowDistanceField(bool show);
    static bool GetDebugShowDistanceField();

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
    //   world position xyz, w = 1 (RGBA32F; the clear of 0 marks empty
    //   pixels), SV_Target3 emission rgb (R11G11B10 float, emitted light,
    //   independent of albedo, added once by the emission pass),
    //   SV_Target4 occlusion (RG8: R foreground opacity, G wall opacity; only
    //   the channels in DeferredGPassShaderOptions::occlusionWriteMask)
    // - material slots (t/s 0-2, space2): 0 albedo, 1 normal map (RGBA8
    //   tangent space: rgb = normal * 0.5 + 0.5, a = normal strength, i.e.
    //   the blend from flat (0, 0, 1) to the map normal), 2 emission map
    //   (rgb = emitted light, e.g. RGBA8). DrawQuad binds 1x1 defaults to slots a quad lacks
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
    // G-buffer (one render pass, each G-pass shader in creation order),
    // then builds the distance field from its occlusion texture (jump
    // flood seed pass, flood passes, distance pass; see BuildDistanceField).
    static void DrawObjects();
    // Lighting pass: adds the emission, draws the lights (shadowed through
    // the distance field), then the queued debug lines on top of the lit
    // image (one render pass).
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
    // Queues a light (see LightComponent / LightType). `type` is the
    // LightType's value: 0 = Point (unlimited cone), 1 = Diffuse. Kept for
    // simple lights; use the LightComponent overload for spotlights and
    // Directional (sun) lights.
    static void DrawLight(const glm::vec3 &position, const glm::vec4 &color,
                          float radius, float intensity, float falloff,
                          float type);
    // Queues a light described by a LightComponent at `position` (the
    // entity's world position; ignored by Directional lights, which cover
    // the whole padded view). Disabled lights are skipped.
    static void DrawLight(const LightComponent &light,
                          const glm::vec3 &position);

    ///////////////////////
    /// PARALLAX LAYERS ///
    ///////////////////////
    // Queues a background layer for this frame (see ParallaxLayerComponent).
    // DrawLights draws every queued layer into the lit image where the
    // G-pass drew nothing (PositionWS.w == 0), after the lights, sorted
    // back to front by m_Depth, alpha blended; then clears the queue.
    // Disabled layers or layers without a loadable texture are skipped.
    static void DrawParallaxLayer(const ParallaxLayerComponent &layer);
    // colour multiplied into layers by their m_SunLitFactor (RGBA, default
    // white), e.g. the sun's colour * intensity for a day / night cycle
    static void SetParallaxSunColor(const glm::vec4 &color);
    static glm::vec4 GetParallaxSunColor();

    ////////////////////////////////
    /// G-BUFFER POST PROCESSING ///
    ////////////////////////////////
    // An ordered stack of PostProcessEffects run by DrawObjects right after
    // the G-pass and BEFORE the distance field / lighting, so they can
    // change anything the lighting reads. Effects run front (index 0) to
    // back; each one sees the G-buffer as left by the previous effects.
    // Each effect is a full-screen pass reading every G-buffer texture
    // (t0 albedo, t1 normal/uv, t2 position, t3 emission, t4 occlusion,
    // padded size, use Load) and writing the ONE target it declares
    // (PostProcessEffect::GetGBufferOutput). It is rendered into a scratch
    // texture of that target's format, which is then swapped with the
    // target (Texture::SwapGPUTexture), so a pass never reads the texture
    // it writes. The shader must write every texel (copy the input where it
    // changes nothing). Disabled / invalid effects are skipped.
    //
    // Adds `effect` at `index` (-1 = the end, the last to run). Returns the
    // index it ended up at.
    static int AddGBufferPostProcess(const Ref<PostProcessEffect> &effect,
                                     int index = -1);
    static void RemoveGBufferPostProcess(const Ref<PostProcessEffect> &effect);
    // moves an effect already in the stack to `index` (clamped)
    static void MoveGBufferPostProcess(const Ref<PostProcessEffect> &effect,
                                       int index);
    static void ClearGBufferPostProcesses();
    // the stack itself, to inspect or rearrange freely
    static std::vector<Ref<PostProcessEffect>> &GetGBufferPostProcesses();

  private:
    // runs s_GBufferPostProcesses (in DrawObjects, before the distance
    // field)
    static void RunGBufferPostProcesses();
    static Ref<Texture> GetGBufferTexture(GBufferTarget target);
    static void CreateParallaxState(int maxLayers);
    static void DrawQueuedParallaxLayers();
    // couple of helpers to separate the code
    static void CreateLightingState(int maxQuads);
    static void CreateLineState(int maxLines);
    static void CreateEmissionState();
    static void CreateDefaultMaps();
    static void CreateJumpFloodStates();
    // jump flood seed + flood passes + distance pass (end of DrawObjects)
    static void BuildDistanceField();
    // queues one light quad with every vertex attribute
    static void QueueLight(const glm::vec3 &position, const glm::vec4 &color,
                           float radius, float intensity, float falloff,
                           float type, const glm::vec4 &directionCone);
    // the bindable a G-pass quad is queued with: every material slot (0-2)
    // filled, defaults where missing
    static Ref<Bindable> GetGPassBindable(const Ref<Bindable> &bindable);
};
} // namespace Pyxis
