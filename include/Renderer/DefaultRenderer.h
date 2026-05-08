#pragma once
#include <Renderer/Renderer.h>

namespace Pyxis {
// The default renderer draws directly to NDC and doesn't use a camera.
class DefaultRenderer {
  private:
    static uint32_t m_TexturePipeline;
    static Ref<Material> m_DefaultMaterial;
    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 tint;
    };

    static const std::vector<TextureVertex> s_TexturedQuadVertices;

  public:
    static bool Init();
    // draws directly to screen output to NDC.
    static void Draw();

    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Material> material = nullptr);
};
} // namespace Pyxis
