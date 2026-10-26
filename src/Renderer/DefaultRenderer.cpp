#include <Renderer/DefaultRenderer.h>
#include <SDL3/SDL_gpu.h>
#include <UI/UINineSlice.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace Pyxis {

std::unique_ptr<PipelineState> DefaultRenderer::s_TextureState = nullptr;
std::unique_ptr<BatchBuffer> DefaultRenderer::s_TextureBatch = nullptr;
Ref<Texture> DefaultRenderer::s_WhiteTexture = nullptr;
float DefaultRenderer::s_UICornerPixelSize = 1.0f;
std::unordered_map<std::string, Ref<Texture>>
    DefaultRenderer::s_ParallaxTextures = {};

void DefaultRenderer::SetUICornerPixelSize(float pixelSize) {
    s_UICornerPixelSize = std::max(pixelSize, 1.0f);
}

const std::vector<DefaultRenderer::TextureVertex>
    DefaultRenderer::s_TexturedQuadVertices{
        {{-0.5f, 0.5f, 0.0f}, {0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}},  // tl
        {{0.5f, 0.5f, 0.0f}, {1, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}},   // tr
        {{-0.5f, -0.5f, 0.0f}, {0, 1}, {1, 1, 1, 1}, {0, 0, 0, 0}}, // bl
        {{0.5f, -0.5f, 0.0f}, {1, 1}, {1, 1, 1, 1}, {0, 0, 0, 0}}   // br
    };
void DefaultRenderer::Init(int maxQuads) {
    PX_BEGINSTEPS("Initializing Default Renderer...");

    s_WhiteTexture =
        Texture::CreateTexture("assets/textures/white.png", "white texture");
    PX_VERIFY(s_WhiteTexture != nullptr,
              "Failed to make default white texture!");

    PipelineStateDescription description;
    description.name = "DefaultTexture";
    description.vertexShaderPath = "assets/shaders/TextureVertex.hlsl";
    description.fragmentShaderPath = "assets/shaders/TextureFragment.hlsl";
    description.vertexStride = sizeof(TextureVertex);
    description.vertexAttributes = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
         .offset = offsetof(TextureVertex, position)},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         .offset = offsetof(TextureVertex, uv)},
        {.location = 2,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, tint)},
        {.location = 3,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, overlay)},
        {.location = 4,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, shapeRect)},
        {.location = 5,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, cornerRadii)},
        {.location = 6,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, borderWidths)},
        {.location = 7,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = offsetof(TextureVertex, shapeParams)},
    };
    description.colorTargets = {
        {.format = Renderer::GetSwapchainTextureFormat(),
         .blend = BlendStates::Alpha()}};
    // UI and the composited world are transparent-capable 2D layers: no
    // depth buffer (no depth test / write), ordered back to front instead
    description.depth.hasDepthTarget = false;
    s_TextureState = std::make_unique<PipelineState>(description);

    s_TextureBatch = std::make_unique<BatchBuffer>(
        "DefaultTexture", (uint32_t)sizeof(TextureVertex), 4 * maxQuads,
        6 * maxQuads, BatchBuffer::DrawOrder::BackToFront);
}

void DefaultRenderer::Shutdown() {
    s_WhiteTexture = nullptr;
    s_ParallaxTextures.clear();
    s_TextureBatch = nullptr;
    s_TextureState = nullptr;
    PX_LOG("Default Renderer Shut Down");
}

void DefaultRenderer::Draw() {
    RenderPassDescription pass;
    pass.name = "DefaultRenderer";
    RenderPassColorAttachment swapchain;
    swapchain.useSwapchain = true;
    swapchain.loadOp = SDL_GPU_LOADOP_CLEAR;
    swapchain.clearColor = {255 / 255.0f, 219 / 255.0f, 187 / 255.0f,
                            255 / 255.0f};
    pass.colorAttachments.push_back(swapchain);
    if (Renderer::BeginPass(pass)) {
        Renderer::Draw(*s_TextureState, *s_TextureBatch);
        Renderer::EndPass();
    }
    s_TextureBatch->Clear();
}

void DefaultRenderer::DrawParallax(const ParallaxComponent &parallax,
                                   const glm::vec2 &cameraPosition,
                                   const glm::vec2 &viewWorldSize,
                                   float depth) {
    if (!parallax.m_Enabled || viewWorldSize.x <= 0 || viewWorldSize.y <= 0)
        return;
    // back to front: higher m_Depth first (equal z keeps queue order)
    std::vector<const ParallaxLayer *> layers;
    for (const ParallaxLayer &layer : parallax.m_Layers)
        if (layer.m_Enabled)
            layers.push_back(&layer);
    std::stable_sort(layers.begin(), layers.end(),
                     [](const ParallaxLayer *a, const ParallaxLayer *b) {
                         return a->m_Depth > b->m_Depth;
                     });
    const glm::vec2 halfView = viewWorldSize * 0.5f;
    for (const ParallaxLayer *layer : layers) {
        Ref<Texture> texture = layer->m_Texture;
        if (texture == nullptr && !layer->m_TexturePath.empty()) {
            auto found = s_ParallaxTextures.find(layer->m_TexturePath);
            if (found == s_ParallaxTextures.end()) {
                Ref<Texture> loaded = Texture::CreateTexture(
                    layer->m_TexturePath, layer->m_TexturePath, false);
                if (loaded == nullptr || loaded->GetGPUTexture() == nullptr) {
                    PX_ERROR("Parallax layer texture {} failed to load",
                             layer->m_TexturePath);
                    loaded = nullptr;
                }
                found = s_ParallaxTextures
                            .emplace(layer->m_TexturePath, loaded)
                            .first;
            }
            texture = found->second;
        }
        if (texture == nullptr)
            continue;
        glm::vec2 tile = layer->m_TileWorldSize;
        const glm::vec2 textureSize = glm::vec2(texture->GetSize());
        if (tile.x <= 0)
            tile.x = textureSize.x;
        if (tile.y <= 0)
            tile.y = textureSize.y;

        // layer space (+y up, texture top-left at 0) of the screen center;
        // continuous, so the layer scrolls smoothly (sub world-texel)
        const glm::vec2 center =
            cameraPosition * layer->m_ParallaxFactor - layer->m_Offset;
        // screen rect in layer space: left, right, bottom, top
        float left = center.x - halfView.x, right = center.x + halfView.x;
        float bottom = center.y - halfView.y, top = center.y + halfView.y;
        // a non-repeating axis only covers its one tile ([0, tile.x] on x,
        // [-tile.y, 0] on y)
        if (!layer->m_RepeatX) {
            left = std::max(left, 0.0f);
            right = std::min(right, tile.x);
        }
        if (!layer->m_RepeatY) {
            bottom = std::max(bottom, -tile.y);
            top = std::min(top, 0.0f);
        }
        if (right <= left || top <= bottom)
            continue;
        // NDC quad (center + size) and its uvs (+v down; > 1 wraps through
        // the texture's repeat sampler)
        const glm::vec2 ndcMin = glm::vec2(left - center.x, bottom - center.y) /
                                 halfView;
        const glm::vec2 ndcMax = glm::vec2(right - center.x, top - center.y) /
                                 halfView;
        const glm::vec4 uvBounds = {left / tile.x, -top / tile.y,
                                    right / tile.x, -bottom / tile.y};
        DrawQuad({(ndcMin + ndcMax) * 0.5f, depth}, ndcMax - ndcMin, texture,
                 layer->m_Tint, uvBounds);
    }
}

void DefaultRenderer::DrawQuad(glm::vec3 position, glm::vec2 size,
                               Ref<Bindable> bindable, const glm::vec4 &tint,
                               const glm::vec4 &uvBounds) {
    QueueQuad(position, size, std::move(bindable), tint, uvBounds,
              {0, 0, 0, 0});
}

void DefaultRenderer::QueueQuad(const glm::vec3 &position,
                                const glm::vec2 &size, Ref<Bindable> bindable,
                                const glm::vec4 &tint,
                                const glm::vec4 &uvBounds,
                                const glm::vec4 &overlay,
                                const UIShape *shape) {
    std::vector<TextureVertex> vertices;

    vertices.push_back( // tl
        {(s_TexturedQuadVertices[0].position * glm::vec3(size, 1)) + position,
         {uvBounds.x, uvBounds.y},
         tint,
         overlay});
    vertices.push_back( // tr
        {(s_TexturedQuadVertices[1].position * glm::vec3(size, 1)) + position,
         {uvBounds.z, uvBounds.y},
         tint,
         overlay});
    vertices.push_back( // bl
        {(s_TexturedQuadVertices[2].position * glm::vec3(size, 1)) + position,
         {uvBounds.x, uvBounds.w},
         tint,
         overlay});
    vertices.push_back( // br
        {(s_TexturedQuadVertices[3].position * glm::vec3(size, 1)) + position,
         {uvBounds.z, uvBounds.w},
         tint,
         overlay});

    if (shape != nullptr && shape->size.x > 0 && shape->size.y > 0) {
        // corner radii clamped to half the shorter side (like CSS / Clay)
        const float maxRadius = std::min(shape->size.x, shape->size.y) * 0.5f;
        const glm::vec4 radii =
            glm::clamp(shape->cornerRadii, glm::vec4(0), glm::vec4(maxRadius));
        const glm::vec2 corners[4] = {{0, 0},
                                      {shape->size.x, 0},
                                      {0, shape->size.y},
                                      {shape->size.x, shape->size.y}};
        for (int i = 0; i < 4; i++) {
            vertices[i].shapeRect = {corners[i], shape->size};
            vertices[i].cornerRadii = radii;
            vertices[i].borderWidths = shape->borderWidths;
            vertices[i].shapeParams = {s_UICornerPixelSize, 1, 0, 0};
        }
    }

    if (bindable == nullptr)
        s_TextureBatch->Queue(vertices, QuadIndices, s_WhiteTexture,
                              position.z);
    else
        s_TextureBatch->Queue(vertices, QuadIndices, bindable, position.z);
}

void DefaultRenderer::DrawText(Ref<Font> font, glm::vec3 position,
                               const std::string &text, const glm::vec4 &color,
                               const glm::vec2 scale) {
    QueueText(font, position, text, color, scale, {0, 0, 0, 0});
}

void DefaultRenderer::QueueText(Ref<Font> font, const glm::vec3 &position,
                                const std::string &text, const glm::vec4 &color,
                                const glm::vec2 &scale,
                                const glm::vec4 &overlay) {
    auto commands = Text::DrawText(font, position, text, color, scale);
    for (auto &c : commands) {
        QueueQuad(glm::vec3(c.position, position.z), c.size, font->GetTexture(),
                  color, c.uvBounds, overlay);
    }
}

namespace {

// UI layout pixels (origin top left) -> the default renderer's NDC quads
struct UIQuadWriter {
    glm::vec2 resolution;
    float depth;
    glm::vec4 overlay;

    // queues the layout rect (x, y, w, h) with the given uv bounds
    void Rect(float x, float y, float w, float h, const Ref<Bindable> &bindable,
              const glm::vec4 &tint, const glm::vec4 &uvBounds = {0, 0, 1, 1},
              const DefaultRenderer::UIShape *shape = nullptr) const;
};

// Clay corner radii -> UIShape order (tl, tr, br, bl); true if any > 0
bool ClayCornerRadii(const Clay_CornerRadius &r, glm::vec4 &radii) {
    radii = {r.topLeft, r.topRight, r.bottomRight, r.bottomLeft};
    return radii.x > 0 || radii.y > 0 || radii.z > 0 || radii.w > 0;
}

// overlay `top` composed over overlay `bottom` (both: rgb mixed in by a)
glm::vec4 ComposeOverlay(const glm::vec4 &bottom, const glm::vec4 &top) {
    const float alpha = 1.0f - (1.0f - bottom.a) * (1.0f - top.a);
    if (alpha <= 0.0f)
        return {0, 0, 0, 0};
    glm::vec3 rgb = (glm::vec3(bottom) * bottom.a * (1.0f - top.a) +
                     glm::vec3(top) * top.a) /
                    alpha;
    return {rgb, alpha};
}

// Appends quads filling [start, start + length) along one axis with a
// source span [uvStart, uvEnd] of `tileLength` layout pixels per repeat:
// (position, length, uvStart, uvEnd) per piece.
struct AxisPiece {
    float position, length, uvStart, uvEnd;
};
void FillAxis(float start, float length, float uvStart, float uvEnd,
              UINineSliceFill fill, float tileLength,
              std::vector<AxisPiece> &pieces) {
    pieces.clear();
    if (length <= 0.0f)
        return;
    if (fill == UINineSliceFill::Stretch || tileLength <= 0.0f) {
        pieces.push_back({start, length, uvStart, uvEnd});
        return;
    }
    // at most this many tiles per axis (a tiny tile on a huge box)
    constexpr int c_MaxTiles = 1024;
    tileLength = std::max(tileLength, length / c_MaxTiles);
    for (float offset = 0.0f; offset < length - 0.001f; offset += tileLength) {
        const float pieceLength = std::min(tileLength, length - offset);
        const float fraction = pieceLength / tileLength;
        pieces.push_back({start + offset, pieceLength, uvStart,
                          uvStart + (uvEnd - uvStart) * fraction});
    }
}

void DrawNineSlice(const UINineSlice &nineSlice, const Clay_BoundingBox &box,
                   const UIQuadWriter &writer) {
    if (nineSlice.texture == nullptr || box.width <= 0 || box.height <= 0)
        return;
    const glm::vec2 textureSize = nineSlice.texture->GetSize();
    if (textureSize.x <= 0 || textureSize.y <= 0)
        return;
    const float scale = std::max(nineSlice.scale, 0.0f);
    const float left = (float)std::max(nineSlice.insetLeft, 0);
    const float right = (float)std::max(nineSlice.insetRight, 0);
    const float top = (float)std::max(nineSlice.insetTop, 0);
    const float bottom = (float)std::max(nineSlice.insetBottom, 0);

    // corners shrink proportionally when the box is smaller than them
    const float cornerWidth = (left + right) * scale;
    const float cornerHeight = (top + bottom) * scale;
    const float fitX = cornerWidth > box.width ? box.width / cornerWidth : 1;
    const float fitY =
        cornerHeight > box.height ? box.height / cornerHeight : 1;

    // destination column / row edges (layout pixels) and source uv edges
    const float xs[4] = {box.x, box.x + left * scale * fitX,
                         box.x + box.width - right * scale * fitX,
                         box.x + box.width};
    const float ys[4] = {box.y, box.y + top * scale * fitY,
                         box.y + box.height - bottom * scale * fitY,
                         box.y + box.height};
    const float us[4] = {0, left / textureSize.x, 1 - right / textureSize.x, 1};
    const float vs[4] = {0, top / textureSize.y, 1 - bottom / textureSize.y, 1};
    // source size of the middle column / row, in layout pixels at `scale`
    const float middleTileWidth = (textureSize.x - left - right) * scale;
    const float middleTileHeight = (textureSize.y - top - bottom) * scale;

    Ref<Bindable> bindable = nineSlice.texture;
    std::vector<AxisPiece> columns, rows;
    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 3; column++) {
            const bool center = row == 1 && column == 1;
            if (center && !nineSlice.drawCenter)
                continue;
            // corners never tile; edges tile along their length only
            UINineSliceFill fillX = UINineSliceFill::Stretch;
            UINineSliceFill fillY = UINineSliceFill::Stretch;
            if (center) {
                fillX = fillY = nineSlice.centerFill;
            } else if (column == 1) { // top / bottom edge
                fillX = nineSlice.edgeFill;
            } else if (row == 1) { // left / right edge
                fillY = nineSlice.edgeFill;
            }
            FillAxis(xs[column], xs[column + 1] - xs[column], us[column],
                     us[column + 1], fillX, middleTileWidth, columns);
            FillAxis(ys[row], ys[row + 1] - ys[row], vs[row], vs[row + 1],
                     fillY, middleTileHeight, rows);
            for (const AxisPiece &r : rows)
                for (const AxisPiece &c : columns)
                    writer.Rect(c.position, r.position, c.length, r.length,
                                bindable, nineSlice.tint,
                                {c.uvStart, r.uvStart, c.uvEnd, r.uvEnd});
        }
    }
}

} // namespace

void UIQuadWriter::Rect(float x, float y, float w, float h,
                        const Ref<Bindable> &bindable, const glm::vec4 &tint,
                        const glm::vec4 &uvBounds,
                        const DefaultRenderer::UIShape *shape) const {
    if (w <= 0 || h <= 0)
        return;
    glm::vec3 center = {(x + w * 0.5f) / resolution.x * 2.0f - 1.0f,
                        -((y + h * 0.5f) / resolution.y * 2.0f - 1.0f), depth};
    glm::vec2 size = {w / resolution.x * 2.0f, h / resolution.y * 2.0f};
    DefaultRenderer::QueueQuad(center, size, bindable, tint, uvBounds, overlay,
                               shape);
}

void DefaultRenderer::DrawUICommands(Clay_RenderCommandArray &renderCommands) {

    glm::vec3 resolution = {Renderer::GetResolution(), 1};

    // clip rects (render target pixels) and overlays of the open
    // SCISSOR / OVERLAY_COLOR commands; the back is the active one
    std::vector<BatchBuffer::ScissorRect> scissorStack;
    std::vector<glm::vec4> overlayStack;
    // layout pixels -> swapchain pixels (the scissor works in target pixels)
    glm::vec2 layoutToTarget = {1, 1};
    if (Renderer::IsFrameActive() &&
        Renderer::GetFrameData().SwapchainSize.x > 0 && resolution.x > 0 &&
        resolution.y > 0)
        layoutToTarget = glm::vec2(Renderer::GetFrameData().SwapchainSize) /
                         glm::vec2(resolution);

    // An image element with a backgroundColor gets both an IMAGE command
    // (the color as its tint) and a RECTANGLE command of that color from
    // Clay, the rectangle painted over the image. The color is the image's
    // tint, so that rectangle is skipped.
    std::vector<uint32_t> imageIds;
    for (int i = 0; i < renderCommands.length; i++) {
        const Clay_RenderCommand &command = renderCommands.internalArray[i];
        if (command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE)
            imageIds.push_back(command.id);
    }

    for (int i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = &renderCommands.internalArray[i];

        // Clay emits the commands in painter's order (floating roots are
        // already sorted by zIndex), so the command index alone gives the
        // depth: 0.9 (first, back) -> 0.4 (last, front). The zIndex itself
        // can't be used: commands emitted when an element closes (BORDER,
        // SCISSOR_END) carry zIndex 0 even inside a floating element.
        const float depth =
            0.9f - ((float)i / (float)renderCommands.length) * 0.5f;
        const glm::vec4 overlay =
            overlayStack.empty() ? glm::vec4(0) : overlayStack.back();
        const UIQuadWriter writer{glm::vec2(resolution), depth, overlay};

        auto &bb = renderCommand->boundingBox;
        glm::vec3 bbCenter = glm::vec3(bb.x, bb.y, 0) +
                             glm::vec3(bb.width / 2.0f, bb.height / 2.0f, 0);
        bbCenter /= resolution;
        bbCenter = (bbCenter * 2.0f) - 1.0f;
        bbCenter.y *= -1.0f;
        bbCenter.z = depth;
        Clay_Color c = renderCommand->renderData.rectangle.backgroundColor;
        glm::vec4 color = {c.r, c.g, c.b, c.a};

        glm::vec3 bbScale = {bb.width, bb.height, 1};
        bbScale /= resolution;
        bbScale *= 2;

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            PX_ERROR("Was given render command of type none!");
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            if (std::find(imageIds.begin(), imageIds.end(),
                          renderCommand->id) != imageIds.end())
                break; // an image's tint, see above
            UIShape shape;
            if (ClayCornerRadii(
                    renderCommand->renderData.rectangle.cornerRadius,
                    shape.cornerRadii)) {
                shape.size = {bb.width, bb.height};
                QueueQuad(bbCenter, bbScale, nullptr, color, {0, 0, 1, 1},
                          overlay, &shape);
            } else {
                QueueQuad(bbCenter, bbScale, nullptr, color, {0, 0, 1, 1},
                          overlay);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            const Clay_BorderRenderData &border =
                renderCommand->renderData.border;
            const glm::vec4 borderColor = {border.color.r, border.color.g,
                                           border.color.b, border.color.a};
            const float left = border.width.left, right = border.width.right;
            const float top = border.width.top, bottom = border.width.bottom;
            UIShape ring;
            if (ClayCornerRadii(border.cornerRadius, ring.cornerRadii)) {
                // rounded: one quad masked to the border ring
                ring.size = {bb.width, bb.height};
                ring.borderWidths = {left, right, top, bottom};
                QueueQuad(bbCenter, bbScale, nullptr, borderColor, {0, 0, 1, 1},
                          overlay, &ring);
                break;
            }
            // top and bottom span the full width, left / right in between
            writer.Rect(bb.x, bb.y, bb.width, top, nullptr, borderColor);
            writer.Rect(bb.x, bb.y + bb.height - bottom, bb.width, bottom,
                        nullptr, borderColor);
            writer.Rect(bb.x, bb.y + top, left, bb.height - top - bottom,
                        nullptr, borderColor);
            writer.Rect(bb.x + bb.width - right, bb.y + top, right,
                        bb.height - top - bottom, nullptr, borderColor);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData trd = renderCommand->renderData.text;
            glm::vec4 color = {trd.textColor.r, trd.textColor.g,
                               trd.textColor.b, trd.textColor.a};
            auto pos = glm::vec3{bb.x, bb.y, 0} /
                       glm::vec3(Renderer::GetResolution(), 1);
            pos = (pos * 2.0f) - 1.0f;
            pos.y = -pos.y;
            // UTF-8, decoded by Text
            std::string s(trd.stringContents.chars,
                          (size_t)trd.stringContents.length);
            glm::vec2 size =
                glm::vec2{(float)trd.fontSize, (float)trd.fontSize};
            size /= Renderer::GetResolution();

            QueueText(Font::GetFontByID(trd.fontId), {pos.x, pos.y, depth}, s,
                      color, size, overlay);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            const Clay_ImageRenderData &image = renderCommand->renderData.image;
            Bindable *b = (Bindable *)image.imageData;
            // backgroundColor tints the image; 0,0,0,0 (Clay's default)
            // means untinted
            const Clay_Color &t = image.backgroundColor;
            const glm::vec4 tint =
                (t.r == 0 && t.g == 0 && t.b == 0 && t.a == 0)
                    ? glm::vec4(1)
                    : glm::vec4(t.r, t.g, t.b, t.a);
            UIShape shape;
            const bool rounded =
                ClayCornerRadii(image.cornerRadius, shape.cornerRadii);
            shape.size = {bb.width, bb.height};
            QueueQuad(bbCenter, bbScale, b->shared_from_this(), tint,
                      {0, 0, 1, 1}, overlay, rounded ? &shape : nullptr);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            // the clip box in target pixels, intersected with the open clip
            int x0 = (int)std::floor(bb.x * layoutToTarget.x);
            int y0 = (int)std::floor(bb.y * layoutToTarget.y);
            int x1 = (int)std::ceil((bb.x + bb.width) * layoutToTarget.x);
            int y1 = (int)std::ceil((bb.y + bb.height) * layoutToTarget.y);
            if (!scissorStack.empty()) {
                const BatchBuffer::ScissorRect &outer = scissorStack.back();
                x0 = std::max(x0, outer.x);
                y0 = std::max(y0, outer.y);
                x1 = std::min(x1, outer.x + outer.w);
                y1 = std::min(y1, outer.y + outer.h);
            }
            BatchBuffer::ScissorRect rect{x0, y0, std::max(x1 - x0, 0),
                                          std::max(y1 - y0, 0)};
            scissorStack.push_back(rect);
            s_TextureBatch->SetScissor(rect);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            if (!scissorStack.empty())
                scissorStack.pop_back();
            if (scissorStack.empty())
                s_TextureBatch->ClearScissor();
            else
                s_TextureBatch->SetScissor(scissorStack.back());
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START: {
            const Clay_Color &o = renderCommand->renderData.overlayColor.color;
            overlayStack.push_back(
                ComposeOverlay(overlay, glm::vec4(o.r, o.g, o.b, o.a)));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            if (!overlayStack.empty())
                overlayStack.pop_back();
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            void *customData = renderCommand->renderData.custom.customData;
            if (customData == nullptr)
                break;
            const UICustomElement *element =
                static_cast<const UICustomElement *>(customData);
            switch (element->type) {
            case UICustomElementType::NineSlice:
                DrawNineSlice(*static_cast<const UINineSlice *>(element), bb,
                              writer);
                break;
            default:
                PX_WARN("Unknown UI custom element type {}",
                        (uint32_t)element->type);
                break;
            }
            break;
        }
        }
    }
    // unbalanced commands must not leak a clip into the rest of the frame
    s_TextureBatch->ClearScissor();
}

} // namespace Pyxis
