#include <Renderer/Renderer.h>
#include <Renderer/Text.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>

namespace Pyxis {

// ============================================================================
// GlyphAtlas Implementation
// ============================================================================

GlyphAtlas::GlyphAtlas(SDL_GPUDevice *device,
                       SDL_GPUCommandBuffer *commandBuffer, TTF_Font *font,
                       uint32_t fontSize)
    : m_Device(device), m_Font(font), m_FontSize(fontSize), m_CurrentX(0),
      m_CurrentY(0), m_RowHeight(0) {

    // Load glyphs as their own surfaces first
    std::unordered_map<uint32_t, SDL_Surface *> glyphSurfaces;
    std::unordered_map<uint32_t, int> glyphAdvances;
    std::unordered_map<uint32_t, glm::ivec2> glyphBearings;

    // ' ' to '~' aka 32 to 126, main ascii visible characters
    uint32_t total_width = 0, max_height = 0;
    for (uint32_t ch = 32; ch < 127; ch++) {
        SDL_Surface *glyphSurface =
            TTF_RenderGlyph_Blended(font, ch, SDL_Color(255, 255, 255, 255));
        if (glyphSurface == nullptr) {
            PX_WARN("Failed to render glyph {}: {}", ch, SDL_GetError());
            continue;
        }

        // Get glyph metrics
        int advance, minx, miny, maxy, maxx;
        if (!TTF_GetGlyphMetrics(m_Font, ch, &minx, &maxx, &miny, &maxy,
                                 &advance)) {
            PX_WARN("Failed to get glyph metrics for {}: {}", ch,
                    SDL_GetError());
            SDL_DestroySurface(glyphSurface);
            continue;
        }

        // Store glyph data for later packing
        glyphSurfaces[ch] = glyphSurface;
        glyphAdvances[ch] = advance;
        glyphBearings[ch] = glm::ivec2(minx, maxy - glyphSurface->h);

        total_width += glyphSurface->w + 1; // +1 for padding
        max_height = std::max((uint32_t)glyphSurface->h, max_height);
    }

    if (glyphSurfaces.empty()) {
        PX_ERROR("Failed to load any glyphs for font!");
        return;
    }

    // Calculate atlas dimensions once (max width 2048, expand vertically as
    // needed)
    const uint32_t MAX_ATLAS_WIDTH = 2048;
    uint32_t atlasWidth = std::min(total_width, MAX_ATLAS_WIDTH);
    uint32_t atlasHeight =
        ((total_width + atlasWidth - 1) / atlasWidth) * (max_height + 1);
    m_AtlasSize = glm::ivec2(atlasWidth, atlasHeight);

    // Create atlas surface with RGBA format
    SDL_Surface *atlasSurface =
        SDL_CreateSurface(atlasWidth, atlasHeight, SDL_PIXELFORMAT_RGBA8888);
    if (atlasSurface == nullptr) {
        PX_ERROR("Failed to create atlas surface: {}", SDL_GetError());
        for (auto &pair : glyphSurfaces) {
            SDL_DestroySurface(pair.second);
        }
        return;
    }

    // Fill atlas surface with transparent black
    SDL_FillSurfaceRect(atlasSurface, nullptr,
                        SDL_MapSurfaceRGBA(atlasSurface, 0, 0, 0, 0));

    // Pack all glyphs into the atlas
    for (auto &glyphPair : glyphSurfaces) {
        uint32_t codepoint = glyphPair.first;
        SDL_Surface *glyphSurface = glyphPair.second;
        int advance = glyphAdvances[codepoint];
        glm::ivec2 bearing = glyphBearings[codepoint];

        // Pack the glyph surface into the atlas
        if (!PackGlyphSurface(atlasSurface, glyphSurface, codepoint, bearing,
                              advance)) {
            PX_WARN("Failed to pack glyph {}", codepoint);
        }
    }

    // Create the atlas texture
    m_Texture =
        CreateRef<Texture>(device, m_AtlasSize,
                           std::string(TTF_GetFontStyleName(font)) + "_atlas");
    if (m_Texture == nullptr) {
        PX_ERROR("Failed to create glyph atlas texture: {}", SDL_GetError());
        SDL_DestroySurface(atlasSurface);
        for (auto &pair : glyphSurfaces) {
            SDL_DestroySurface(pair.second);
        }
        return;
    }

    // Upload atlas texture data to GPU
    m_Texture->SetTextureData(device, commandBuffer, atlasSurface->pixels);

    // also set material so that the text pipeline knows the font
    m_Material = CreateRef<Material>();
    m_Material->SetTexture(0, m_Texture);

    // Get font metrics
    m_LineHeight = TTF_GetFontHeight(m_Font);
    m_Baseline = TTF_GetFontAscent(m_Font);

    PX_LOG("Created glyph atlas {}x{} for font size {}", m_AtlasSize.x,
           m_AtlasSize.y, fontSize);

    // Clean up atlas surface
    SDL_DestroySurface(atlasSurface);

    // Clean up glyph surfaces
    for (auto &pair : glyphSurfaces) {
        SDL_DestroySurface(pair.second);
    }
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

bool GlyphAtlas::PackGlyphSurface(SDL_Surface *atlasSurface,
                                  SDL_Surface *glyphSurface, uint32_t codepoint,
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

    // Convert glyph surface to match atlas format (RGBA8888)
    SDL_Surface *convertedSurface =
        SDL_ConvertSurface(glyphSurface, SDL_PIXELFORMAT_RGBA8888);
    if (convertedSurface == nullptr) {
        PX_ERROR("Failed to convert glyph surface: {}", SDL_GetError());
        return false;
    }

    // Copy glyph surface into atlas at the current position
    SDL_Rect srcRect{0, 0, (int)glyphWidth, (int)glyphHeight};
    SDL_Rect dstRect{(int)m_CurrentX, (int)m_CurrentY, (int)glyphWidth,
                     (int)glyphHeight};

    if (!SDL_BlitSurface(convertedSurface, &srcRect, atlasSurface, &dstRect)) {
        PX_ERROR("Failed to blit glyph surface to atlas: {}", SDL_GetError());
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    glm::ivec2 atlasPos(m_CurrentX, m_CurrentY);

    // Create glyph entry with all required data
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

    // Store the complete glyph in the cache
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

    // Create a command buffer for texture upload
    SDL_GPUCommandBuffer *commandBuffer =
        SDL_AcquireGPUCommandBuffer(s_GPUDevice);
    if (commandBuffer == nullptr) {
        PX_ERROR("Failed to acquire GPU command buffer: {}", SDL_GetError());
        TTF_CloseFont(font);
        return -1;
    }

    // Create glyph atlas for this font
    GlyphAtlas *atlas =
        new GlyphAtlas(s_GPUDevice, commandBuffer, font, fontSize);

    // Submit the command buffer for texture upload
    if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
        PX_ERROR("Failed to submit GPU command buffer: {}", SDL_GetError());
        delete atlas;
        TTF_CloseFont(font);
        return -1;
    }

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

Ref<Material> Text::GetFontMaterial(int fontID) {
    auto fontData = s_Fonts.find(fontID);
    if (fontData == s_Fonts.end()) {
        PX_WARN("Attempted to get material for font ID {} which doesn't exist",
                fontID);
        return nullptr;
    }
    return fontData->second.atlas->GetMaterial();
}
Ref<Texture> Text::GetFontTexture(int fontID) {
    auto fontData = s_Fonts.find(fontID);
    if (fontData == s_Fonts.end()) {
        PX_WARN("Attempted to get material for font ID {} which doesn't exist",
                fontID);
        return nullptr;
    }
    return fontData->second.atlas->GetTexture();
}

GlyphAtlas *Text::GetGlyphAtlas(int fontID) {
    auto it = s_Fonts.find(fontID);
    if (it == s_Fonts.end()) {
        PX_WARN(
            "Attempted to get glyph atlas for font ID {} which doesn't exist",
            fontID);
        return nullptr;
    }
    return it->second.atlas;
}

std::vector<Text::GlyphCommand>
Text::DrawText(int fontID, const glm::vec2 &position, const std::string &text,
               const glm::vec4 &color, const glm::vec2 &scale) {
    std::vector<Text::GlyphCommand> result;

    auto fontData = s_Fonts.find(fontID);
    if (fontData == s_Fonts.end()) {
        PX_ERROR("Font ID {} not found!", fontID);
        return result;
    }

    GlyphAtlas *atlas = fontData->second.atlas;

    glm::vec2 currentPos = position;
    currentPos.y -= ((float)fontData->second.atlas->GetLineHeight() * scale.y);

    // Generate vertices for each character
    for (char c : text) {
        uint32_t codepoint = static_cast<unsigned char>(c);
        const Glyph *glyph = atlas->GetGlyph(codepoint);

        if (c == ' ') {
            const Glyph *glyph = atlas->GetGlyph(codepoint);
            currentPos.x += glyph->advance * scale.x;
            continue;
        }

        if (glyph == nullptr) {
            // Skip characters that can't be rendered
            PX_WARN("Skipping char queue as we couldn't find the glyph");
            continue;
        }

        // Calculate glyph position with bearing
        glm::vec2 glyphPos =
            currentPos +
            (glm::vec2(glyph->bearing.x, glyph->bearing.y) * scale);
        glyphPos += glm::vec2(glyph->size) * scale * 0.5f;
        result.push_back({.position = glyphPos,
                          .size = (glm::vec2)glyph->size * scale,
                          .uvBounds = glyph->uvBounds});

        // Advance to next character position
        currentPos.x += glyph->advance * scale.x;
    }

    return result;
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
Clay_Dimensions Text::Clay_MeasureText(Clay_StringSlice text,
                                       Clay_TextElementConfig *config,
                                       void *userData) {
    auto fontIter = s_Fonts.find(config->fontId);
    if (fontIter == s_Fonts.end()) {
        PX_ERROR("Font ID {} not found!", config->fontId);
        return {0, 0};
    }

    GlyphAtlas *atlas = fontIter->second.atlas;
    glm::vec2 size(0, atlas->GetLineHeight());

    std::string s;
    float width = 0;
    for (int i = 0; i < text.length; i++) {
        char c = text.chars[i];
        s.push_back(c);
        uint32_t codepoint = static_cast<unsigned char>(c);
        const Glyph *glyph = atlas->GetGlyph(codepoint);
        if (glyph != nullptr) {
            width += glyph->advance;
        }
    }

    size.x = width;
    PX_TRACE("measured '{}', length {}, size {}", s, text.length,
             size * (float)config->fontSize * 0.5f);
    return {
        size.x * config->fontSize * 0.5f,
        size.y * config->fontSize *
            0.5f}; // halved due to drawing on clip
                   // space (atleast i think , due to * 2 - 1) sorry future me
}

} // namespace Pyxis
