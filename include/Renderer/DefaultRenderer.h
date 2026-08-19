#pragma once
#include "Renderer/UI.h"
#include <Renderer/Renderer.h>

namespace Pyxis {
// The default renderer draws directly to NDC and doesn't use a camera.
// In SDL3, the depth is from 0 (near) to 1 (far), and otherwise -1 to 1 on x,y
class DefaultRenderer {
  public:
    static Ref<Texture> s_WhiteTexture;
    static Ref<Material> s_DefaultMaterial;

  private:
    static int s_TexturePipelineID;
    static Ref<Texture> s_DepthTexture;

    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 tint;
    };

    static const std::vector<TextureVertex> s_TexturedQuadVertices;

  public:
    static bool Init(int maxQuads = 10000);
    static void Shutdown();

    // call this when the screen resizes to update depth texture size
    static void Resize(const glm::ivec2 &resolution);

    // draws directly to screen output to NDC.
    static void Draw();

    /// uv bounds are xmin, ymin, xmax, ymax. leaving material null will use
    /// white texture material.
    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Material> material = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1});
    static void DrawText(int fontID, glm::vec3 position,
                         const std::string &text,
                         const glm::vec4 &color = {1, 1, 1, 1},
                         const glm::vec2 scale = {1, 1});
    static void DrawUICommands(Clay_RenderCommandArray &renderCommands);
};
} // namespace Pyxis
