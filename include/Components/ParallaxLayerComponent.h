#pragma once
#include <Core/Core.h>
#include <glm/glm.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <string>

namespace Pyxis {

class Texture;

// A repeating background image drawn behind the world (sky, mountains, ...).
// Put it on any entity and queue it every frame with
// DeferredRenderer::DrawParallaxLayer(layer) before DrawLights. Layers are
// NOT lit through the G-buffer: they are composited into the lit image only
// where nothing was drawn in the G-pass (PositionWS.w == 0, i.e. sky /
// empty pixels), sorted back to front by m_Depth and alpha blended.
struct ParallaxLayerComponent {
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ParallaxLayerComponent, m_Enabled,
                                   m_TexturePath, m_ParallaxFactor, m_Tint,
                                   m_Offset, m_TileWorldSize, m_RepeatX,
                                   m_RepeatY, m_Depth, m_SunLitFactor);

    bool m_Enabled = true;
    // png loaded (and cached) by the DeferredRenderer the first time the
    // layer is drawn. Ignored when m_Texture is set.
    std::string m_TexturePath;
    // optional texture set directly from code (not serialized; overrides
    // m_TexturePath)
    Ref<Texture> m_Texture = nullptr;

    // how much the layer follows the camera, per axis: 0 = fixed to the
    // screen (infinitely far away), 1 = moves with the world, 0..1 = in
    // between (farther layers use smaller values)
    glm::vec2 m_ParallaxFactor = {0.5f, 0.5f};
    // RGBA multiplier (alpha fades the whole layer)
    glm::vec4 m_Tint = {1, 1, 1, 1};
    // world units the layer is shifted by (layer space; +y = up). The
    // texture's top-left corner sits at -m_Offset when the camera is at 0.
    glm::vec2 m_Offset = {0, 0};
    // world size of ONE repetition of the texture. 0 on an axis = the
    // texture's size in pixels (1 texel = 1 world unit).
    glm::vec2 m_TileWorldSize = {0, 0};
    // repeat the texture along x / y; a non-repeating axis clamps to the
    // edge texels (e.g. the sky colour above a horizon strip extends up)
    bool m_RepeatX = true;
    bool m_RepeatY = false;
    // draw order: higher = farther back, drawn first
    float m_Depth = 0.0f;
    // 0 = unaffected, 1 = fully multiplied by the renderer's parallax sun
    // colour (DeferredRenderer::SetParallaxSunColor), so layers can follow a
    // day / night cycle
    float m_SunLitFactor = 0.0f;
};

} // namespace Pyxis
