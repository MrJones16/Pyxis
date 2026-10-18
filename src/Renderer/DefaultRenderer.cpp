#include <Renderer/DefaultRenderer.h>
#include <SDL3/SDL_gpu.h>
#include <cstddef>
#include <memory>

namespace Pyxis {

std::unique_ptr<PipelineState> DefaultRenderer::s_TextureState = nullptr;
std::unique_ptr<BatchBuffer> DefaultRenderer::s_TextureBatch = nullptr;
Ref<Texture> DefaultRenderer::s_WhiteTexture = nullptr;

const std::vector<DefaultRenderer::TextureVertex>
    DefaultRenderer::s_TexturedQuadVertices{
        {{-0.5f, 0.5f, 0.0f}, {0, 0}, {1, 1, 1, 1}},  // tl
        {{0.5f, 0.5f, 0.0f}, {1, 0}, {1, 1, 1, 1}},   // tr
        {{-0.5f, -0.5f, 0.0f}, {0, 1}, {1, 1, 1, 1}}, // bl
        {{0.5f, -0.5f, 0.0f}, {1, 1}, {1, 1, 1, 1}}   // br
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
    };
    description.colorTargets = {{.format = Renderer::GetSwapchainTextureFormat(),
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

void DefaultRenderer::DrawQuad(glm::vec3 position, glm::vec2 size,
                               Ref<Bindable> bindable, const glm::vec4 &tint,
                               const glm::vec4 &uvBounds) {
    std::vector<TextureVertex> vertices;

    vertices.push_back( // tl
        {(s_TexturedQuadVertices[0].position * glm::vec3(size, 1)) + position,
         {uvBounds.x, uvBounds.y},
         tint});
    vertices.push_back( // tr
        {(s_TexturedQuadVertices[1].position * glm::vec3(size, 1)) + position,
         {uvBounds.z, uvBounds.y},
         tint});
    vertices.push_back( // bl
        {(s_TexturedQuadVertices[2].position * glm::vec3(size, 1)) + position,
         {uvBounds.x, uvBounds.w},
         tint});
    vertices.push_back( // br
        {(s_TexturedQuadVertices[3].position * glm::vec3(size, 1)) + position,
         {uvBounds.z, uvBounds.w},
         tint});

    if (bindable == nullptr)
        s_TextureBatch->Queue(vertices, QuadIndices, s_WhiteTexture,
                              position.z);
    else
        s_TextureBatch->Queue(vertices, QuadIndices, bindable, position.z);
}

void DefaultRenderer::DrawText(Ref<Font> font, glm::vec3 position,
                               const std::string &text, const glm::vec4 &color,
                               const glm::vec2 scale) {

    auto commands = Text::DrawText(font, position, text, color, scale);
    for (auto &c : commands) {
        DrawQuad(glm::vec3(c.position, position.z), c.size, font->GetTexture(),
                 color, c.uvBounds);
    }
}

void DefaultRenderer::DrawUICommands(Clay_RenderCommandArray &renderCommands) {

    glm::vec3 resolution = {Renderer::GetResolution(), 1};

    for (int i = 0; i < renderCommands.length; i++) {
        Clay_RenderCommand *renderCommand = &renderCommands.internalArray[i];

        float depth;
        if (renderCommand->zIndex == 0) {
            // index was not set, so lets render from 0.9 -> 0.8
            depth = 0.9 - (((float)i / (float)renderCommands.length) * 0.1f);
        } else {
            // index was set, so render at specific depth in 0.7->0.2 range
            depth = 0.9 - (((float)renderCommand->zIndex / 1000.0f) * 0.5f);
        }

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

            // ... Implement handling of other command types
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            PX_ERROR("Was given render command of type none!");
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            DrawQuad(bbCenter, bbScale, nullptr, color);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData trd = renderCommand->renderData.text;
            glm::vec4 color = {trd.textColor.r, trd.textColor.g,
                               trd.textColor.b, trd.textColor.a};
            auto pos = glm::vec3{bb.x, bb.y, 0} /
                       glm::vec3(Renderer::GetResolution(), 1);
            pos = (pos * 2.0f) - 1.0f;
            pos.y = -pos.y;
            std::string s;
            for (int i = 0; i < trd.stringContents.length; i++) {
                s.push_back(trd.stringContents.chars[i]);
            }
            glm::vec2 size =
                glm::vec2{(float)trd.fontSize, (float)trd.fontSize};
            size /= Renderer::GetResolution();

            DrawText(Font::GetFontByID(trd.fontId), pos, s, color, size);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Bindable *b = (Bindable *)renderCommand->renderData.image.imageData;
            DrawQuad(bbCenter, bbScale, b->shared_from_this(), {1, 1, 1, 1});
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:

            PX_WARN(
                "Tried to render clay SCISSOR START which is not implemented.");
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            PX_WARN(
                "Tried to render clay SCISSOR END which is not implemented.");
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            PX_WARN("Tried to render clay OVERLAY COLOR START which is not "
                    "implemented.");
            break;
        case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            PX_WARN("Tried to render clay OVERLAY COLOR END which is not "
                    "implemented.");
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            PX_WARN("Tried to render clay CUSTOM which is not implemented.");
            break;
        }
    }
}

} // namespace Pyxis
