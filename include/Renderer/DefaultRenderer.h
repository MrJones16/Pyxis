#pragma once
#include <Core/UI.h>
#include <Renderer/BatchBuffer.h>
#include <Renderer/PipelineState.h>
#include <Renderer/Text.h>
#include <memory>

namespace Pyxis {
// The default renderer draws direcly to NDC and doesn't use a
// camera. In SDL3, the depth is from 0 (near) to 1 (far), and otherwise -1 to 1
// on x,y
class DefaultRenderer {
  public:
    static Ref<Texture> s_WhiteTexture;

  private:
    // textured quads straight to the swapchain, alpha blended, no depth
    // buffer: the batch draws back to front by z (see BatchBuffer)
    static std::unique_ptr<PipelineState> s_TextureState;
    static std::unique_ptr<BatchBuffer> s_TextureBatch;

    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 tint;
    };

    static const std::vector<TextureVertex> s_TexturedQuadVertices;

  public:
    static void Init(int maxQuads = 10000);
    static void Shutdown();

    // Draws everything queued this frame to the swapchain (NDC), in one
    // render pass, then clears the queue. Call between BeginFrame/EndFrame.
    // Quads are alpha blended and drawn back to front by their z (0 = front,
    // 1 = back); equal z keeps the order they were queued in.
    static void Draw();

    /// uv bounds are xmin, ymin, xmax, ymax. leaving material null will use
    /// white texture material.
    static void DrawQuad(glm::vec3 position, glm::vec2 size,
                         Ref<Bindable> bindable = nullptr,
                         const glm::vec4 &tint = {1, 1, 1, 1},
                         const glm::vec4 &uvBounds = {0, 0, 1, 1});
    static void DrawText(Ref<Font> font, glm::vec3 position,
                         const std::string &text,
                         const glm::vec4 &color = {1, 1, 1, 1},
                         const glm::vec2 scale = {1, 1});
    static void DrawUICommands(Clay_RenderCommandArray &renderCommands);
};
} // namespace Pyxis
