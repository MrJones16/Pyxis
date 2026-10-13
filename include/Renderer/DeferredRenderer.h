#pragma once
#include <Components/CameraComponent.h>
#include <Renderer/DefaultRenderer.h>

namespace Pyxis {
// The default renderer draws directly to NDC and doesn't use a camera.
// In SDL3, the depth is from 0 (near) to 1 (far), and otherwise -1 to 1 on x,y
class DeferredRenderer {
  private:
    static glm::ivec2 s_RenderResolution;
    // background color of the scene (G-buffer albedo clear color)
    static glm::vec4 s_ClearColor;
    // draws things to G buffer
    static Pipeline *s_TexturePipeline;
    // draws lights onto G buffer
    static Pipeline *s_LightingPipeline;
    // draws unlit debug lines on top of the lit image (line list)
    static Pipeline *s_LinePipeline;

    // we can use basic renderer's default material since we aren't using

    static Ref<Uniform> s_CameraUniform;

    static Ref<Material> s_GBufferMaterial;
    static Ref<Texture> s_GTextureColor;
    static Ref<Texture> s_GTexturePositionNS;
    static Ref<Texture> s_GTextureNormalUV;

    static Ref<Texture> s_LightingTexture;
    static Ref<Material> s_LightingTextureMaterial;

    static Ref<Texture> s_DepthTexture;

    struct DeferredTextureVertex {
        glm::vec4 color;
        glm::vec4 normal_uv;
        glm::vec4 position_ns;
    };

    struct DeferredLineVertex {
        glm::vec4 color;
        glm::vec4 position; // world position (xyz), w unused
    };

    // something like this, tbd
    struct DeferredLightVertex {
        glm::vec4 color;
        glm::vec4 position;
        glm::vec4 positionCenter;
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

    static void DrawObjects(Renderer::FrameData &frameData);
    // Draws the lights, then the queued debug lines on top of the lit image.
    static void DrawLights(Renderer::FrameData &frameData);

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

    /// uv bounds are xmin, ymin, xmax, ymax. leaving material null will use
    /// white texture material.
    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Bindable> bindable = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const float normalStrength = 0.5f);
    static void DrawQuad(const glm::mat4 &transform,
                         const glm::vec2 &size = {1, 1},
                         Ref<Material> material = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const float normalStrength = 0.5f);
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
    static void CreateTexturePipeline(int maxQuads);
    static void CreateLightingPipeline(int maxQuads);
    static void CreateLinePipeline(int maxLines);
};
} // namespace Pyxis
