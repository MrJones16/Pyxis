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
        // UI color overlay: rgb mixed over the result by a (0 = none)
        glm::vec4 overlay;
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
    // DrawQuad / DrawText with a color overlay (rgb mixed over the result
    // by a; used by the UI's OVERLAY_COLOR commands)
    static void QueueQuad(const glm::vec3 &position, const glm::vec2 &size,
                          Ref<Bindable> bindable, const glm::vec4 &tint,
                          const glm::vec4 &uvBounds, const glm::vec4 &overlay);
    static void QueueText(Ref<Font> font, const glm::vec3 &position,
                          const std::string &text, const glm::vec4 &color,
                          const glm::vec2 &scale, const glm::vec4 &overlay);
    // Queues Clay's render commands (colors are 0..1 RGBA in Pyxis):
    // RECTANGLE, BORDER (4 rects, per side widths), TEXT (UTF-8), IMAGE
    // (imageData = Bindable*), SCISSOR_START/END (clip / scroll
    // containers; nested clips intersect), OVERLAY_COLOR_START/END (the
    // overlay rgb is mixed over the element and its children by its alpha;
    // nested overlays compose) and CUSTOM elements from UI/UINineSlice.h.
    // Corner radii are not drawn.
    static void DrawUICommands(Clay_RenderCommandArray &renderCommands);
};
} // namespace Pyxis
