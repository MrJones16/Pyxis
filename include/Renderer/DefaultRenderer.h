#pragma once
#include <Components/ParallaxComponent.h>
#include <Core/UI.h>
#include <Renderer/BatchBuffer.h>
#include <Renderer/PipelineState.h>
#include <Renderer/Text.h>
#include <memory>
#include <unordered_map>

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
        // UI shape (see UIShape): position inside the shape rect (UI
        // pixels) xy, shape size zw
        glm::vec4 shapeRect;
        // corner radii: top left, top right, bottom right, bottom left
        glm::vec4 cornerRadii;
        // border widths (0 = filled): left, right, top, bottom
        glm::vec4 borderWidths;
        // x = corner pixel size, y = 1 if the shape is used
        glm::vec4 shapeParams;
    };

    static float s_UICornerPixelSize;

    // textures loaded from ParallaxLayer::m_TexturePath (null cached too)
    static std::unordered_map<std::string, Ref<Texture>> s_ParallaxTextures;


    static const std::vector<TextureVertex> s_TexturedQuadVertices;

  public:
    // A rounded-rect mask for a UI quad, evaluated per pixel in the UI
    // fragment shader with a hard edge (no anti-aliasing). Pixels outside
    // the rounded rect are discarded; with border widths > 0 only the
    // border ring (outer rounded rect minus the inset inner one, whose
    // radii shrink by the border widths) is kept.
    struct UIShape {
        glm::vec2 size = {0, 0}; // UI pixels, = the quad's size
        // top left, top right, bottom right, bottom left (UI pixels)
        glm::vec4 cornerRadii = {0, 0, 0, 0};
        // left, right, top, bottom (UI pixels); all 0 = filled shape
        glm::vec4 borderWidths = {0, 0, 0, 0};
    };

    // Size (UI pixels) of the grid the rounded corners are evaluated on,
    // anchored at each element's top left: 1 = per screen pixel, e.g. 4 for
    // pixel art drawn at 4x so corners step in whole art pixels. Default 1.
    static void SetUICornerPixelSize(float pixelSize);
    static float GetUICornerPixelSize() { return s_UICornerPixelSize; }

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
    // Queues every enabled layer of `parallax` as a textured quad (at z
    // `depth`, back to front by ParallaxLayer::m_Depth) covering the screen,
    // in full output resolution with continuous scrolling: the layer's uv
    // shift is cameraPosition * m_ParallaxFactor / tile size (no rounding).
    // `viewWorldSize` is the world size the screen shows (ortho camera
    // size). Repeating axes wrap through the texture's repeat sampler; a
    // non-repeating axis draws the texture once (its tile band).
    // Queue it before DeferredRenderer::DrawToScreen with the same z so the
    // world (opaque where drawn) covers it.
    static void DrawParallax(const ParallaxComponent &parallax,
                             const glm::vec2 &cameraPosition,
                             const glm::vec2 &viewWorldSize, float depth = 1);
    static void DrawText(Ref<Font> font, glm::vec3 position,
                         const std::string &text,
                         const glm::vec4 &color = {1, 1, 1, 1},
                         const glm::vec2 scale = {1, 1});
    // DrawQuad / DrawText with a color overlay (rgb mixed over the result
    // by a; used by the UI's OVERLAY_COLOR commands)
    // `shape` (optional) masks the quad to a rounded rect / ring.
    static void QueueQuad(const glm::vec3 &position, const glm::vec2 &size,
                          Ref<Bindable> bindable, const glm::vec4 &tint,
                          const glm::vec4 &uvBounds, const glm::vec4 &overlay,
                          const UIShape *shape = nullptr);
    static void QueueText(Ref<Font> font, const glm::vec3 &position,
                          const std::string &text, const glm::vec4 &color,
                          const glm::vec2 &scale, const glm::vec4 &overlay);
    // Queues Clay's render commands (colors are 0..1 RGBA in Pyxis):
    // RECTANGLE, BORDER (4 rects, per side widths), TEXT (UTF-8), IMAGE
    // (imageData = Bindable*), SCISSOR_START/END (clip / scroll
    // containers; nested clips intersect), OVERLAY_COLOR_START/END (the
    // overlay rgb is mixed over the element and its children by its alpha;
    // nested overlays compose) and CUSTOM elements from UI/UINineSlice.h.
    // Corner radii are drawn for RECTANGLE, BORDER and IMAGE (crisp, see
    // SetUICornerPixelSize); IMAGE uses its backgroundColor as a tint when
    // it isn't 0,0,0,0 (Clay's "untinted" default). 9-slices ignore the
    // corner radius.
    static void DrawUICommands(Clay_RenderCommandArray &renderCommands);
};
} // namespace Pyxis
