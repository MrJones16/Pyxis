#include <Renderer/Renderer.h>
#include <Renderer/Text.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstring>
#include <vector>

namespace Pyxis {

// ============================================================================
// GlyphAtlas Implementation
// ============================================================================

GlyphAtlas::GlyphAtlas(SDL_GPUDevice *device, TTF_Font *font, uint32_t fontSize)
    : m_Device(device), m_Font(font), m_FontSize(fontSize), m_CurrentX(0),
      m_CurrentY(0), m_RowHeight(0) {

    // load the glyphs as their own surfaces first
    std::unordered_map<uint32_t, SDL_Surface *> glyphSurfaces;

    // ' ' to '~' aka 32 to 126, main ascii visible characters
    uint32_t total_width, max_height;
    for (uint32_t ch = 32; ch < 127; ch++) {
        SDL_Surface *glyphSurface =
            TTF_RenderGlyph_Blended(font, ch, SDL_Color(1, 1, 1, 1));
        if (glyphSurface == nullptr) {

            PX_WARN("Failed to render glyph {}: {}", ch, SDL_GetError());
            SDL_DestroySurface(glyphSurface);
            continue;
        }
        glyphSurfaces[ch] = glyphSurface;
        m_Glyphs[ch] = Glyph{};
        total_width += glyphSurface->w + 1;
        max_height = std::max((uint32_t)glyphSurface->h, max_height);

        // Get glyph metrics
        int advance, minx, miny, maxy, maxx;
        if (!TTF_GetGlyphMetrics(m_Font, ch, &minx, &maxx, &miny, &maxy,
                                 &advance)) {
            PX_WARN("Failed to get glyph metrics for {}: {}", ch,
                    SDL_GetError());
            SDL_DestroySurface(glyphSurface);
            continue;
        }

        glm::ivec2 bearing(minx, maxy - glyphSurface->h);

        PICKUP WORK HERE
    }

    // Create the atlas texture
    m_Texture = CreateRef<Texture>(device, TEXTURE SIZE HERE,
                                   TTF_GetFontStyleName(font));
    if (m_Texture == nullptr) {
        PX_ERROR("Failed to create glyph atlas texture: {}", SDL_GetError());
    }

    // Get font metrics
    m_LineHeight = TTF_GetFontHeight(m_Font);
    m_Baseline = TTF_GetFontAscent(m_Font);

    PX_LOG("Created glyph atlas {}x{} for font size {}", m_AtlasSize.x,
           m_AtlasSize.y, fontSize);
}

GlyphAtlas::~GlyphAtlas() {
    m_Texture = nullptr; // lose reference to texture
    m_Glyphs.clear();
}

const Glyph *GlyphAtlas::GetGlyph(uint32_t codePoint) {
    // Check if glyph already exists in cache
    auto it = m_Glyphs.find(codePoint);
    if (it != m_Glyphs.end()) {
        return &it->second;
    }
    PX_WARN("Tried getting unknown glyph!");
    return nullptr;
}

bool GlyphAtlas::PackGlyphSurface(SDL_Surface *glyphSurface, uint32_t codepoint,
                                  glm::ivec2 bearing, int advance) {
    uint32_t glyphWidth = glyphSurface->w;
    uint32_t glyphHeight = glyphSurface->h;

    // Check if glyph fits on current row
    if (m_CurrentX + glyphWidth > m_AtlasSize.x) {
        // Move to next row
        m_CurrentY += m_RowHeight + 1; // +1 for padding
        m_CurrentX = 0;
        m_RowHeight = 0;
    }

    // Check if glyph fits vertically
    if (m_CurrentY + glyphHeight > m_AtlasSize.y) {
        PX_ERROR("Glyph atlas is full! Cannot pack glyph {}", codepoint);
        return false;
    }

    // Update row height
    if (glyphHeight > m_RowHeight) {
        m_RowHeight = glyphHeight;
    }

    // Create a temporary surface with RGBA format for the atlas
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(glyphSurface, SDL_PIXELFORMAT_RGBA8888);
    if (convertedSurface == nullptr) {
        PX_ERROR("Failed to convert glyph surface: {}", SDL_GetError());
        return false;
    }

    // TODO: Upload the glyph surface to the atlas texture
    // For now, we'll just track the glyph metadata
    // In a full implementation, you would use SDL_UpdateGPUTexture or
    // similar

    glm::ivec2 atlasPos(m_CurrentX, m_CurrentY);

    // Create glyph entry
    Glyph glyph{};
    glyph.atlasPosition = atlasPos;
    glyph.size = glm::ivec2(glyphWidth, glyphHeight);
    glyph.bearing = bearing;
    glyph.advance = advance;

    // Calculate normalized UV bounds
    float minU = static_cast<float>(atlasPos.x) / m_AtlasSize.x;
    float minV = static_cast<float>(atlasPos.y) / m_AtlasSize.y;
    float maxU = static_cast<float>(atlasPos.x + glyphWidth) / m_AtlasSize.x;
    float maxV = static_cast<float>(atlasPos.y + glyphHeight) / m_AtlasSize.y;
    glyph.uvBounds = glm::vec4(minU, minV, maxU, maxV);

    m_Glyphs[codepoint] = glyph;

    // Advance packing position
    m_CurrentX += glyphWidth + 1; // +1 for padding

    SDL_DestroySurface(convertedSurface);

    PX_LOG("Packed glyph {} at atlas position ({}, {})", codepoint, atlasPos.x,
           atlasPos.y);

    return true;
}

// ============================================================================
// Text Implementation
// ============================================================================

SDL_GPUDevice *Text::s_GPUDevice = nullptr;
std::unordered_map<int, Text::FontData> Text::s_Fonts = {};
int Text::s_NextFontID = 0;
int Text::s_TextPipelineID = -1;

bool Text::Init(SDL_GPUDevice *device) {
    if (device == nullptr) {
        PX_ERROR("Cannot initialize Text system with nullptr device!");
        return false;
    }

    s_GPUDevice = device;

    if (!TTF_Init()) {
        PX_ERROR("Failed to initialize SDL3_ttf: {}", SDL_GetError());
        return false;
    }

    // Create text pipeline on first font load
    if (s_TextPipelineID < 0) {
        // Create text vertex attributes
        std::vector<SDL_GPUVertexAttribute> vertexAttributes;

        // Position attribute (vec3)
        vertexAttributes.push_back(
            SDL_GPUVertexAttribute{.location = 0,
                                   .buffer_slot = 0,
                                   .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                                   .offset = offsetof(TextVertex, position)});

        // UV attribute (vec2)
        vertexAttributes.push_back(
            SDL_GPUVertexAttribute{.location = 1,
                                   .buffer_slot = 0,
                                   .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                   .offset = offsetof(TextVertex, uv)});

        // Color attribute (vec4)
        vertexAttributes.push_back(
            SDL_GPUVertexAttribute{.location = 2,
                                   .buffer_slot = 0,
                                   .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                                   .offset = offsetof(TextVertex, color)});

        // Create color target description
        std::vector<SDL_GPUColorTargetDescription> colorTargetDescriptions;
        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_color_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTarget.blend_state.dst_color_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.src_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTarget.blend_state.dst_alpha_blendfactor =
            SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.format = Renderer::GetSwapchainTextureFormat();
        colorTargetDescriptions.push_back(colorTarget);

        // Create color target info
        std::vector<SDL_GPUColorTargetInfo> colorTargetInfos;
        SDL_GPUColorTargetInfo colorTargetInfo{};
        colorTargetInfo.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
        colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        colorTargetInfos.push_back(colorTargetInfo);

        PX_LOG("Loading first font, going to initialize font pipeline!");
        // Create the text pipeline
        s_TextPipelineID = Renderer::CreatePipeline(
            6 * 10000, // Max vertices (enough for ~1666 characters)
            sizeof(TextVertex), vertexAttributes, colorTargetDescriptions,
            colorTargetInfos, "assets/shaders/text_vertex.hlsl",
            "assets/shaders/text_fragment.hlsl", true);

        if (s_TextPipelineID < 0) {
            PX_ERROR("Failed to create text rendering pipeline!");
            return -1;
        }

        PX_LOG("Created text rendering pipeline");
    }

    PX_LOG("Text system initialized");
    return true;
}

void Text::Shutdown() {
    // Unload all fonts
    std::vector<int> fontIDs;
    for (auto &pair : s_Fonts) {
        fontIDs.push_back(pair.first);
    }
    for (int id : fontIDs) {
        UnloadFont(id);
    }

    TTF_Quit();
    s_GPUDevice = nullptr;
    s_TextPipelineID = -1;
    PX_LOG("Text system shut down");
}

int Text::LoadFont(const std::string &fontPath, uint32_t fontSize) {
    if (s_GPUDevice == nullptr) {
        PX_ERROR("Text system not initialized! Call Text::Init first");
        return -1;
    }

    TTF_Font *font =
        TTF_OpenFont(fontPath.c_str(), static_cast<float>(fontSize));
    if (font == nullptr) {
        PX_ERROR("Failed to load font from {}: {}", fontPath, SDL_GetError());
        return -1;
    }

    // Create glyph atlas for this font
    GlyphAtlas *atlas = new GlyphAtlas(s_GPUDevice, font, fontSize);

    int fontID = s_NextFontID++;
    s_Fonts[fontID] = {atlas, font, fontSize};

    PX_LOG("Loaded font {} from {} with size {}", fontID, fontPath, fontSize);
    return fontID;
}

void Text::UnloadFont(int fontID) {
    auto it = s_Fonts.find(fontID);
    if (it == s_Fonts.end()) {
        PX_WARN("Attempted to unload non-existent font ID {}", fontID);
        return;
    }

    delete it->second.atlas;
    TTF_CloseFont(it->second.font);
    s_Fonts.erase(it);
    PX_LOG("Unloaded font {}", fontID);
}

uint32_t Text::QueueText(int fontID, const std::string &text,
                         const glm::vec2 &position, const glm::vec4 &color) {
    if (s_TextPipelineID < 0) {
        PX_ERROR("Text pipeline not initialized!");
        return 0;
    }

    auto fontIt = s_Fonts.find(fontID);
    if (fontIt == s_Fonts.end()) {
        PX_ERROR("Font ID {} not found!", fontID);
        return 0;
    }

    GlyphAtlas *atlas = fontIt->second.atlas;
    std::vector<TextVertex> vertices;

    glm::vec2 currentPos = position;

    // Generate vertices for each character
    for (char c : text) {
        uint32_t codepoint = static_cast<unsigned char>(c);

        const Glyph *glyph = atlas->GetGlyph(codepoint);
        if (glyph == nullptr) {
            // Skip characters that can't be rendered
            continue;
        }

        // Calculate glyph position with bearing
        glm::vec2 glyphPos =
            currentPos + glm::vec2(glyph->bearing.x, -glyph->bearing.y);

        // Create quad vertices (2 triangles)
        float x0 = glyphPos.x;
        float y0 = glyphPos.y;
        float x1 = glyphPos.x + glyph->size.x;
        float y1 = glyphPos.y + glyph->size.y;

        float u0 = glyph->uvBounds.x;
        float v0 = glyph->uvBounds.y;
        float u1 = glyph->uvBounds.z;
        float v1 = glyph->uvBounds.w;

        // First triangle (top-left, bottom-left, top-right)
        vertices.push_back({{x0, y0, 0.0f}, {u0, v0}, color});
        vertices.push_back({{x0, y1, 0.0f}, {u0, v1}, color});
        vertices.push_back({{x1, y0, 0.0f}, {u1, v0}, color});

        // Second triangle (bottom-left, bottom-right, top-right)
        vertices.push_back({{x0, y1, 0.0f}, {u0, v1}, color});
        vertices.push_back({{x1, y1, 0.0f}, {u1, v1}, color});
        vertices.push_back({{x1, y0, 0.0f}, {u1, v0}, color});

        // Advance to next character position
        currentPos.x += glyph->advance;
    }

    // Queue vertices to the text pipeline
    if (!vertices.empty()) {
        Renderer::DrawToPipeline(s_TextPipelineID, vertices);
    }

    return vertices.size();
}

glm::ivec2 Text::GetTextSize(int fontID, const std::string &text) {
    auto fontIt = s_Fonts.find(fontID);
    if (fontIt == s_Fonts.end()) {
        PX_ERROR("Font ID {} not found!", fontID);
        return glm::ivec2(0, 0);
    }

    GlyphAtlas *atlas = fontIt->second.atlas;
    glm::ivec2 size(0, atlas->GetLineHeight());

    int width = 0;
    for (char c : text) {
        uint32_t codepoint = static_cast<unsigned char>(c);
        const Glyph *glyph = atlas->GetGlyph(codepoint);
        if (glyph != nullptr) {
            width += glyph->advance;
        }
    }

    size.x = width;
    return size;
}

} // namespace Pyxis
