#pragma once
#include <Renderer/Texture.h>
#include <UI/UIModule.h>
#include <cstdint>

namespace Pyxis {

// Clay custom elements (Clay_ElementDeclaration::custom.customData) drawn by
// DefaultRenderer::DrawUICommands. Every custom element struct derives from
// UICustomElement so the renderer can tell what it was given; pass it with
// AsCustomData(), never as a pointer to the derived struct directly.
enum class UICustomElementType : uint32_t {
    NineSlice = 1,
};

struct UICustomElement {
    const UICustomElementType type;

    // the pointer to put in Clay_ElementDeclaration::custom.customData
    inline void *AsCustomData() { return static_cast<UICustomElement *>(this); }

  protected:
    explicit UICustomElement(UICustomElementType elementType)
        : type(elementType) {}
    UICustomElement(const UICustomElement &) = default;
    UICustomElement &operator=(const UICustomElement &) { return *this; }
};

// How the edges / center of a 9-slice fill their area.
enum class UINineSliceFill {
    Stretch, // one quad, the source region scaled to fit (default)
    Tile,    // the source region repeated at its own size (x scale); the
             // last tile on each axis is cut off to fit
};

// A texture drawn as a 9-slice ("9-patch") in a Clay element's box:
//
//     +----+------------+----+   the four corners keep their size
//     | TL |    top     | TR |   (inset x scale); the top / bottom edges
//     +----+------------+----+   fill horizontally, the left / right edges
//     |left|   center   |rght|   vertically, the center both ways, each
//     +----+------------+----+   either stretched or tiled.
//     | BL |   bottom   | BR |
//     +----+------------+----+
//
// The insets are in texture pixels, measured from each texture edge. If the
// box is smaller than the corners, the corners shrink proportionally.
//
// Usage (the struct must stay alive until DrawUICommands has run, e.g. keep
// it in your module):
//
//     UINineSlice panel;
//     panel.texture = Texture::CreateTexture("assets/textures/panel.png");
//     panel.insetLeft = panel.insetRight = 4;
//     panel.insetTop = panel.insetBottom = 4;
//     panel.scale = 4; // 1 texture pixel = 4 UI pixels
//     panel.centerFill = UINineSliceFill::Tile;
//     CLAY(CLAY_ID("Panel"),
//          {.layout = {...}, .custom = {.customData = panel.AsCustomData()}}) {
//         ... children are drawn on top of it ...
//     }
//
// or use UINineSliceModule below.
struct UINineSlice : UICustomElement {
    UINineSlice() : UICustomElement(UICustomElementType::NineSlice) {}

    Ref<Texture> texture = nullptr;
    // texture pixels from the left / right / top / bottom texture edge
    int insetLeft = 0;
    int insetRight = 0;
    int insetTop = 0;
    int insetBottom = 0;
    // UI (layout) pixels per texture pixel, for the corners, the edge
    // thickness and the tile size
    float scale = 1.0f;
    UINineSliceFill edgeFill = UINineSliceFill::Stretch;
    UINineSliceFill centerFill = UINineSliceFill::Stretch;
    // color multiplied with the texture (0..1 RGBA)
    glm::vec4 tint = {1, 1, 1, 1};
    // false leaves the center empty (a frame)
    bool drawCenter = true;
};

// A UI panel module whose background is a 9-slice; children are drawn on
// top. Configure m_NineSlice and the layout in m_Config as usual (don't set
// m_Config.custom, it's filled in).
struct UINineSliceModule : UIModule {
    UINineSlice m_NineSlice;

    UINineSliceModule(const std::string &id, Clay_ElementDeclaration config,
                      const UINineSlice &nineSlice = {})
        : UIModule(id, config), m_NineSlice(nineSlice) {}

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;
        m_Config.custom.customData = m_NineSlice.AsCustomData();
        CLAY(GetClayID(), m_Config) {
            for (auto &module : m_Children)
                module->DrawUI();
        }
    }
};

} // namespace Pyxis
