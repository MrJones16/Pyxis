#pragma once
#include <Components/CameraComponent.h>
#include <Renderer/DefaultRenderer.h>

namespace Pyxis {
// The default renderer draws directly to NDC and doesn't use a camera.
// In SDL3, the depth is from 0 (near) to 1 (far), and otherwise -1 to 1 on x,y
class DeferredRenderer {
  private:
    static glm::ivec2 s_RenderResolution;
    // draws things to G buffer
    static int s_TexturePipelineID;
    // draws lights onto G buffer
    static int s_LightingPipelineID;

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

    static void DrawObjects();
    static void DrawLights();
    static void DrawToScreen(float depth = 1);
    static void Debug_DrawColorToScreen();
    static void Debug_DrawNormalUVToScreen();
    static void Debug_DrawPositionToScreen();

    /// uv bounds are xmin, ymin, xmax, ymax. leaving material null will use
    /// white texture material.
    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Material> material = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const float normalStrength = 0.5f);
    static void DrawQuad(const glm::mat4 &transform,
                         const glm::vec2 &size = {1, 1},
                         Ref<Material> material = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1},
                         const float normalStrength = 0.5f);
    static void DrawText(int fontID, glm::vec3 position,
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
};
} // namespace Pyxis
