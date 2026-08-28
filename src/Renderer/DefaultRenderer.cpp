#include <Renderer/DefaultRenderer.h>
#include <SDL3/SDL_gpu.h>
#include <memory>

namespace Pyxis {

Pipeline *DefaultRenderer::s_TexturePipeline = nullptr;
Ref<Texture> DefaultRenderer::s_DepthTexture = nullptr;
Ref<Texture> DefaultRenderer::s_WhiteTexture = nullptr;

const std::vector<DefaultRenderer::TextureVertex>
    DefaultRenderer::s_TexturedQuadVertices{
        {{-0.5f, 0.5f, 0.0f}, {0, 0}, {1, 1, 1, 1}},  // tl
        {{0.5f, 0.5f, 0.0f}, {1, 0}, {1, 1, 1, 1}},   // tr
        {{-0.5f, -0.5f, 0.0f}, {0, 1}, {1, 1, 1, 1}}, // bl
        {{0.5f, -0.5f, 0.0f}, {1, 1}, {1, 1, 1, 1}}   // br
    };
void DefaultRenderer::Init(int maxQuads) {
    s_WhiteTexture =
        Texture::CreateTexture("assets/textures/white.png", "white texture");
    PX_ASSERT(s_WhiteTexture != nullptr,
              "Failed to make default white texture!");

    std::vector<SDL_GPUVertexAttribute> textureVertexAttributes{};
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        // position
        .location = 0,    // layout (location = 0) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, // vec3
        .offset = 0 // start from the first byte from current buffer position

    });
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        // uv
        .location = 1,    // layout (location = 1) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, // vec3
        .offset = sizeof(float) * 3 // 4th float from current buffer position

    });
    textureVertexAttributes.push_back(SDL_GPUVertexAttribute{
        // tint
        .location = 2,    // layout (location = 2) in shader
        .buffer_slot = 0, // fetch data from the buffer at slot 0
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, // vec3
        .offset = sizeof(float) * 5 // 6th float from current buffer position

    });

    std::vector<SDL_GPUColorTargetDescription> TextureColorTargetDescriptions;
    SDL_GPUColorTargetDescription ct1{};
    ct1.blend_state.enable_blend = true;
    ct1.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ct1.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    ct1.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ct1.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct1.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    ct1.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct1.format = Renderer::GetSwapchainTextureFormat();
    TextureColorTargetDescriptions.push_back(ct1);

    SDL_GPUColorTargetInfo colorTargetInfo{};
    // discard previous content and clear to a color
    colorTargetInfo.clear_color = {255 / 255.0f, 219 / 255.0f, 187 / 255.0f,
                                   255 / 255.0f};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
    // colorTargetInfo.texture = leave blank for swapchain target, set
    // otherwise;
    std::vector<SDL_GPUColorTargetInfo> targetInfoVec;
    targetInfoVec.push_back(colorTargetInfo);

    glm::ivec2 resolution = Renderer::GetResolution();
    PX_TRACE("Resolution: {}", resolution);
    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    textureInfo.width = resolution.x;
    textureInfo.height = resolution.y;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    textureInfo.props = 0;
    s_DepthTexture = Texture::CreateTexture(textureInfo, "drdst");
    PX_ASSERT(s_DepthTexture != nullptr, "Failed to create depth texture!");

    SDL_GPUDepthStencilTargetInfo dsti{};
    dsti.texture = dsti.texture = s_DepthTexture->GetGPUTexture();
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_depth = 1.0f; // far plane
    dsti.stencil_load_op = SDL_GPU_LOADOP_LOAD;
    dsti.stencil_store_op = SDL_GPU_STOREOP_STORE;
    dsti.clear_stencil = 0;

    // Create default sprite pipeline as an example & default
    // ~1000 items max
    s_TexturePipeline =
        new Pipeline(4 * maxQuads, sizeof(TextureVertex), 6 * maxQuads,
                     textureVertexAttributes, TextureColorTargetDescriptions,
                     targetInfoVec, &dsti, "assets/shaders/TextureVertex.hlsl",
                     "assets/shaders/TextureFragment.hlsl", true);
    PX_ASSERT(s_TexturePipeline != nullptr,
              "Failed to init default texture pipeline!");
    return;
}

void DefaultRenderer::Shutdown() {
    s_WhiteTexture = nullptr;
    s_DepthTexture = nullptr;
    delete s_TexturePipeline;
    PX_TRACE("Default Renderer Shut Down");
}

void DefaultRenderer::Resize(const glm::ivec2 &resolution) {
    // When we resize the texture, the underlying sdl gpu texture is replaced.
    // This means that the pointer held by the pipeline breaks.
    // This is why I call UpdateDepthStencilTargetTexture.
    s_DepthTexture->Resize(resolution);
    s_TexturePipeline->UpdateDepthStencilTargetTexture(s_DepthTexture);
    s_TexturePipeline->SetResolution(resolution);
}

void DefaultRenderer::Draw(Renderer::FrameData &frameData) {
    s_TexturePipeline->Draw(frameData);
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
        s_TexturePipeline->QueueMesh(vertices, QuadIndices, nullptr);
    else
        s_TexturePipeline->QueueMesh(vertices, QuadIndices, bindable);
}

void DefaultRenderer::DrawText(int fontID, glm::vec3 position,
                               const std::string &text, const glm::vec4 &color,
                               const glm::vec2 scale) {
    Ref<Material> fontMaterial = Text::GetFontMaterial(fontID);
    if (fontMaterial == nullptr)
        return;

    auto commands = Text::DrawText(fontID, position, text, color, scale);
    for (auto &c : commands) {
        DrawQuad(glm::vec3(c.position, position.z), c.size, fontMaterial, color,
                 c.uvBounds);
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
            DrawText(trd.fontId, pos, s, color, size);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Material *m = (Material *)renderCommand->renderData.image.imageData;
            DrawQuad(bbCenter, bbScale, m->get_shared(), {1, 1, 1, 1});
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
