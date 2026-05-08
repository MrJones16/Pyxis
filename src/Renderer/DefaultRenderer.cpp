#include <Renderer/DefaultRenderer.h>

namespace Pyxis {

const std::vector<DefaultRenderer::TextureVertex>
    DefaultRenderer::s_TexturedQuadVertices{
        {{-0.5f, 0.5f, 0.0f}, {0, 0}, {1, 1, 1, 1}},  // tl
        {{0.5f, 0.5f, 0.0f}, {1, 0}, {1, 1, 1, 1}},   // tr
        {{-0.5f, -0.5f, 0.0f}, {0, 1}, {1, 1, 1, 1}}, // bl
        {{0.5f, -0.5f, 0.0f}, {1, 1}, {1, 1, 1, 1}}   // br
    };
bool DefaultRenderer::Init() {
    m_DefaultMaterial = CreateRef<Material>(0);
    auto whiteTexture =
        Renderer::CreateTexture("assets/textures/white.png", "white texture");
    if (m_DefaultMaterial == nullptr || whiteTexture == nullptr)
        return false;
    m_DefaultMaterial->SetTexture(0, whiteTexture);

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

    // Create default sprite pipeline as an example & default
    // ~1000 items max
    m_TexturePipeline = Renderer::CreatePipeline(
        4 * 1000, sizeof(TextureVertex), 6 * 1000, textureVertexAttributes,
        TextureColorTargetDescriptions, targetInfoVec,
        "assets/shaders/TextureVertex.hlsl",
        "assets/shaders/TextureFragment.hlsl", true);
    if (m_TexturePipeline < 0) {
        PX_ERROR("Failed to init default texture pipeline");
        return false;
    }
    return true;
}

void DefaultRenderer::Draw() { Renderer::DrawPipeline(m_TexturePipeline); }

void DefaultRenderer::DrawQuad(glm::vec3 position, glm::vec2 size,
                               Ref<Material> material) {
    std::vector<TextureVertex> vertices;
    for (auto &v : s_TexturedQuadVertices) {
        vertices.push_back(
            {(v.position * glm::vec3(size, 1)) + position, v.uv, v.tint});
    }
    if (material == nullptr)
        Renderer::DrawToPipeline(m_TexturePipeline, s_TexturedQuadVertices,
                                 QuadIndices, m_DefaultMaterial);
    else
        Renderer::DrawToPipeline(m_TexturePipeline, s_TexturedQuadVertices,
                                 QuadIndices, material);
}
} // namespace Pyxis
