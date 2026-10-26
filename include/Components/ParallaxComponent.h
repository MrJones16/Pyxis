#pragma once
#include <Core/Core.h>
#include <glm/glm.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <string>
#include <vector>

namespace Pyxis {

class Texture;

// One repeating background image of a ParallaxComponent.
struct ParallaxLayer {
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ParallaxLayer, m_Enabled,
                                                m_TexturePath,
                                                m_ParallaxFactor, m_Tint,
                                                m_Offset, m_TileWorldSize,
                                                m_RepeatX, m_RepeatY, m_Depth);

    bool m_Enabled = true;
    // png loaded (and cached) by the DefaultRenderer the first time the
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
    // world units the layer is shifted by (+y = up). With the camera at 0
    // the texture's top-left corner sits at world position m_Offset.
    glm::vec2 m_Offset = {0, 0};
    // world size of ONE repetition of the texture. 0 on an axis = the
    // texture's size in pixels (1 texel = 1 world unit).
    glm::vec2 m_TileWorldSize = {0, 0};
    // repeat the texture along x / y; a non-repeating axis draws it once
    bool m_RepeatX = true;
    bool m_RepeatY = false;
    // draw order: higher = farther back, drawn first
    float m_Depth = 0.0f;
};

// A parallax background made of any number of layers (sky, clouds,
// mountains, ...). Drawn by DefaultRenderer::DrawParallax at the full output
// resolution with continuous (not grid-snapped) scrolling. Queue it before
// DeferredRenderer::DrawToScreen at the same z so it ends up behind the
// world (the lit image is transparent where nothing was drawn).
struct ParallaxComponent {
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ParallaxComponent, m_Enabled,
                                                m_Layers);

    bool m_Enabled = true;
    std::vector<ParallaxLayer> m_Layers;
};

} // namespace Pyxis
