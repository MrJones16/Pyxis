#include <Renderer/Renderer.h>
#include <Renderer/Text.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>

namespace Pyxis {

// ============================================================================
// Font Implementation
// ============================================================================

uint16_t Font::s_NextFontID = 1;
std::unordered_map<uint16_t, std::weak_ptr<Font>> Font::s_FontIDs = {};

Ref<Font> Font::GetFontByID(uint16_t ID) {
    if (s_FontIDs.contains(ID)) {
        // shouldnt be possible for this to not be valid
        return s_FontIDs[ID].lock();
    } else {
        PX_THROW_ERROR("Tried to get font with invalid ID");
        return nullptr;
    }
}

Font::Font(const std::string fontPath, uint32_t fontSize)
    : m_FontSize(fontSize) {

    PX_BEGINSTEPS("Creating font from {}", fontPath);

    // initialize vars that track the atlas index
    m_AtlasX = 0;
    m_AtlasY = 0;

    m_Font = TTF_OpenFont(fontPath.c_str(), static_cast<float>(fontSize));
    PX_ASSERT(m_Font != nullptr, "Failed to load font from {}: {}", fontPath,
              SDL_GetError())

    // Get font metrics
    m_LineHeight = TTF_GetFontHeight(m_Font);
    m_Baseline = TTF_GetFontAscent(m_Font);
    m_FontHeight = TTF_GetFontHeight(m_Font);

    m_AtlasSize = glm::ivec2(ATLAS_WIDTH, m_FontHeight);

    // Create atlas surface with RGBA format
    m_AtlasSurface = SDL_CreateSurface(m_AtlasSize.x, m_AtlasSize.y,
                                       SDL_PIXELFORMAT_RGBA8888);
    PX_ASSERT(m_AtlasSurface != nullptr, "Failed to create atlas surface: {}",
              SDL_GetError());

    // Fill atlas surface with transparent black
    SDL_FillSurfaceRect(m_AtlasSurface, nullptr,
                        SDL_MapSurfaceRGBA(m_AtlasSurface, 0, 0, 0, 0));

    // Pack main glyphs into the atlas
    // ' ' to '~' aka 32 to 126, main ascii visible characters
    uint32_t total_width = 0, max_height = 0;
    for (uint32_t ch = 32; ch < 127; ch++) {

        AddCodepoint(ch);
    }

    // Create the renderer texture
    m_Texture = Texture::CreateTexture(
        m_AtlasSize, std::string(TTF_GetFontStyleName(m_Font)) + "_atlas");
    PX_ASSERT(m_Texture != nullptr, "failed to create gpu texture for font");

    // Upload atlas texture data to GPU
    m_Texture->SetTextureData(m_AtlasSurface->pixels);

    // add this font to the global font ids.
    m_FontID = s_NextFontID++;

    PX_STEPSUCCESS("Created glyph atlas {}x{} for font size {}", m_AtlasSize.x,
                   m_AtlasSize.y, fontSize);
    PX_ENDSTEPS();
}

Ref<Font> Font::LoadFont(const std::string fontPath, uint32_t fontSize) {
    Font *f = new Font(fontPath, fontSize);

    Ref<Font> rf =
        std::shared_ptr<Font>(f); // convert privately made font into shared ptr
    s_FontIDs[f->m_FontID] = rf;
    PX_TRACE("Font loaded with ID {}", f->m_FontID);
    return rf;
}

Font::~Font() {
    // remove from global fonts
    s_FontIDs.erase(m_FontID);
    SDL_DestroySurface(m_AtlasSurface);
    m_Texture = nullptr; // clear reference to texture
    m_Glyphs.clear();
}

const Glyph Font::GetGlyph(uint32_t codepoint) {
    // Check if glyph already exists in cache
    auto iter = m_Glyphs.find(codepoint);
    if (iter != m_Glyphs.end()) {
        return iter->second;
    } else {
        // tried getting unpacked glyph
        if (AddCodepoint(codepoint)) {
            UpdateTexture();
            return m_Glyphs.find(codepoint)->second;
        } else {
            PX_THROW_ERROR("Unable to get a glyph for that codepoint! {}",
                           codepoint);
            return Glyph{.atlasPosition = {-1, -1}};
        }
    }
}

bool Font::AddCodepoint(uint32_t codepoint) {
    if (!TTF_FontHasGlyph(m_Font, codepoint)) {
        PX_WARN("Unable to add codepoint {}, its missing from font!",
                codepoint);
        return false;
    }
    int advance, minx, miny, maxy, maxx;
    if (!TTF_GetGlyphMetrics(m_Font, codepoint, &minx, &maxx, &miny, &maxy,
                             &advance)) {
        PX_WARN("Failed to get glyph metrics for {}: {}", codepoint,
                SDL_GetError());
        return false;
    }

    SDL_Surface *glyphSurface =
        TTF_RenderGlyph_Solid(m_Font, codepoint, SDL_Color(255, 255, 255, 255));
    if (glyphSurface == nullptr) {
        PX_WARN("Failed to render glyph {}: {}", codepoint, SDL_GetError());
        return false;
    }

    bool status = PackGlyphSurface(glyphSurface, codepoint, {0, 0}, advance);
    SDL_DestroySurface(glyphSurface);
    return status;
}

bool Font::PackGlyphSurface(SDL_Surface *glyphSurface, uint32_t codepoint,
                            glm::vec2 bearing, int advance) {
    uint32_t glyphWidth = glyphSurface->w;
    uint32_t glyphHeight = glyphSurface->h;

    // Check if glyph fits vertically. should always be true I'd imagine!
    if (glyphHeight > m_FontHeight) {
        PX_THROW_ERROR("Tried packing a glyph that is too tall! it exceeded "
                       "the font height. {}",
                       codepoint);
        return false;
    }

    // Check if glyph fits on current row
    if (m_AtlasX + glyphWidth > m_AtlasSize.x) {
        // Move to next row
        AddRowToAtlas();
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
    SDL_Rect dstRect{(int)m_AtlasX, (int)m_AtlasY, (int)glyphWidth,
                     (int)glyphHeight};

    if (!SDL_BlitSurface(convertedSurface, &srcRect, m_AtlasSurface,
                         &dstRect)) {
        PX_ERROR("Failed to blit glyph surface to atlas: {}", SDL_GetError());
        SDL_DestroySurface(convertedSurface);
        return false;
    }
    SDL_DestroySurface(convertedSurface);

    glm::ivec2 atlasPos(m_AtlasX, m_AtlasY);

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
    m_AtlasX += glyphWidth + 1; // +1 for padding

    PX_LOG("Packed glyph {} at atlas position ({}, {})", codepoint, atlasPos.x,
           atlasPos.y);

    return true;
}

void Font::AddRowToAtlas() {
    SDL_Surface *previousSurface = m_AtlasSurface;
    glm::ivec2 previousSize = m_AtlasSize;

    // Update the new size
    m_AtlasSize.y += m_FontHeight + 1;

    // Create new atlas surface with RGBA format
    m_AtlasSurface = SDL_CreateSurface(m_AtlasSize.x, m_AtlasSize.y,
                                       SDL_PIXELFORMAT_RGBA8888);
    PX_ASSERT(m_AtlasSurface != nullptr, "Failed to create atlas surface: {}",
              SDL_GetError());

    // Fill atlas surface with transparent black
    SDL_FillSurfaceRect(m_AtlasSurface, nullptr,
                        SDL_MapSurfaceRGBA(m_AtlasSurface, 0, 0, 0, 0));

    SDL_Rect rect{0, 0, previousSize.x, previousSize.y};
    if (!SDL_BlitSurface(previousSurface, &rect, m_AtlasSurface, &rect)) {
        PX_ERROR("Failed to add row to atlas: {}", SDL_GetError());
        SDL_DestroySurface(m_AtlasSurface);
        m_AtlasSurface = previousSurface;
        m_AtlasSize = previousSize;
        PX_THROW_ERROR("Failed to add row to atlas: {}", SDL_GetError());
    } else {
        PX_TRACE("Added row to atlas");
        for (auto &g : m_Glyphs) {
            // multiply by old size to go back to pixels instead of normalized
            g.second.uvBounds *= glm::vec4(previousSize, previousSize);
            // divide by new size to get updated normalized values
            g.second.uvBounds /= glm::vec4(m_AtlasSize, m_AtlasSize);
        }
        m_AtlasX = 0;
        m_AtlasY += m_FontHeight + 1;
    }
}

void Font::UpdateTexture() {
    // resize texture, but this only actually does that if it differs :)
    m_Texture->Resize(m_AtlasSize);
    // Upload atlas texture data to GPU
    m_Texture->SetTextureData(m_AtlasSurface->pixels);
}

// ============================================================================
// Text Implementation
// ============================================================================

std::vector<Text::GlyphCommand> Text::DrawText(Ref<Font> font,
                                               const glm::vec2 &position,
                                               const std::string &text,
                                               const glm::vec4 &color,
                                               const glm::vec2 &scale) {
    std::vector<Text::GlyphCommand> result;
    if (font == nullptr) {
        PX_THROW_ERROR("Tried drawing with a null font!");
        return result;
    }

    glm::vec2 currentPos = position;
    currentPos.y -= font->GetLineHeight() * scale.y;
    // currentPos.y -= ((float)fontData->second.atlas->GetLineHeight() *
    // scale.y);

    // Generate vertices for each character
    for (char c : text) {
        uint32_t codepoint = static_cast<unsigned char>(c);
        try {
            const Glyph glyph = font->GetGlyph(codepoint);

            if (c == ' ') {
                currentPos.x += glyph.advance * scale.x;
                continue;
            }

            // Calculate glyph position
            glm::vec2 glyphPos =
                currentPos + glm::vec2(glyph.size) * scale * 0.5f;

            result.push_back({.position = glyphPos,
                              .size = (glm::vec2)glyph.size * scale,
                              .uvBounds = glyph.uvBounds});

            // Advance to next character position
            currentPos.x += glyph.advance * scale.x;
        } catch (std::exception &e) {
            PX_WARN("Skipping glyphcommand for character {}, was not in font!",
                    c);
            continue;
        }
    }

    return result;
}

glm::ivec2 Text::GetTextSize(Ref<Font> font, const std::string &text) {

    glm::ivec2 size(0, font->GetLineHeight());

    int width = 0;
    for (char c : text) {
        uint32_t codepoint = static_cast<unsigned char>(c);
        try {
            const Glyph glyph = font->GetGlyph(codepoint);
            width += glyph.advance;
        } catch (std::exception &e) {
            PX_WARN("Error when getting text size: {}", e.what());
            continue;
        }
    }

    size.x = width;
    return size;
}
Clay_Dimensions Text::Clay_MeasureText(Clay_StringSlice text,
                                       Clay_TextElementConfig *config,
                                       void *userData) {

    Ref<Font> font = Font::GetFontByID(config->fontId);

    glm::vec2 size(0, font->GetLineHeight());

    std::string s;
    float width = 0;
    for (int i = 0; i < text.length; i++) {
        char c = text.chars[i];
        s.push_back(c);
        uint32_t codepoint = static_cast<unsigned char>(c);
        try {
            const Glyph glyph = font->GetGlyph(codepoint);
            width += glyph.advance;
        } catch (std::exception &e) {
            PX_WARN("failed to get text length: {}", e.what());
            return {0, 0};
        }
    }

    size.x = width;
    return {
        size.x * config->fontSize * 0.5f,
        size.y * config->fontSize *
            0.5f}; // halved due to drawing on clip
                   // space (atleast i think , due to * 2 - 1) sorry future me
}

} // namespace Pyxis
