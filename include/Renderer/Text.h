#pragma once

#include <Renderer/Renderer.h>
#include <Renderer/Texture.h>
#include <Renderer/clay.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pyxis {

// Represents a single glyph in the atlas
struct Glyph {
    glm::ivec2 atlasPosition; // Position in the atlas texture
    glm::ivec2 size;          // Width and height of the glyph
    glm::vec2 bearing;        // Offset from baseline (x, y)
    int advance;              // Advance to next character
    glm::vec4
        uvBounds; // Normalized UV coordinates (min_x, min_y, max_x, max_y)
};

// Manages a texture atlas containing glyphs for a single font
class Font : public std::enable_shared_from_this<Font> {
    static uint16_t s_NextFontID;
    static std::unordered_map<uint16_t, std::weak_ptr<Font>> s_FontIDs;

    static const uint32_t ATLAS_WIDTH = 1024;
    Font(const std::string fontPath, uint32_t fontSize);

  public:
    static Ref<Font> GetFontByID(uint16_t ID);
    inline uint16_t GetFontID() { return m_FontID; }

    ~Font();

    // Creates a Font to be referenced by a renderer implementation
    static Ref<Font> LoadFont(const std::string fontAssetPath,
                              uint32_t fontSize);

    // Get or create a glyph in the atlas
    // Will throw a runtime error if it's not able
    // to get the glyph!
    // This will call UpdateTexture automatically if a new glyph is made
    const Glyph GetGlyph(uint32_t codePoint);

    // Get the texture for this atlas
    Ref<Texture> GetTexture() const { return m_Texture; }

    // Get font metrics
    int GetLineHeight() const { return m_LineHeight; }
    int GetBaseline() const { return m_Baseline; }

  protected:
    int m_FontID = 0; // needed for text rendering later as clay passes font id
    TTF_Font *m_Font;
    uint32_t m_FontSize;
    Ref<Texture> m_Texture;

    // Cached glyphs that exist in the atlas
    std::unordered_map<uint32_t, Glyph> m_Glyphs;

    // Font metrics
    int m_LineHeight;
    int m_Baseline;
    int m_FontHeight = 0;

    // Atlas texture and dimensions
    SDL_Surface *m_AtlasSurface;
    glm::ivec2 m_AtlasSize;

    // Current packing position for new glyphs
    uint32_t m_AtlasX;
    uint32_t m_AtlasY;

    // Helper to render a codepoint into the atlas. Uses the pack function
    // below.
    // Does NOT update the underlying texture. You must call UpdateTexture();
    bool AddCodepoint(uint32_t codepoint);

    // Helper to pack a glyph surface into the atlas surface
    bool PackGlyphSurface(SDL_Surface *glyphSurface, uint32_t codepoint,
                          glm::vec2 bearing, int advance);

    void AddRowToAtlas();

    void UpdateTexture();
};

// Vertex format for text rendering
struct TextVertex {
    glm::vec3 position; // x, y, z (z for layering/depth)
    glm::vec2 uv;       // UV coordinates in the glyph atlas
    glm::vec4 color;    // RGBA color
};

// Text rendering system
class Text {
  public:
    Text() = default;
    ~Text();

    struct GlyphCommand {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec4 uvBounds;
    };

    static std::vector<GlyphCommand>
    DrawText(Ref<Font> font, const glm::vec2 &position, const std::string &text,
             const glm::vec4 &color = {1, 1, 1, 1},
             const glm::vec2 &scale = {1, 1});

    // Get text dimensions without rendering
    static glm::ivec2 GetTextSize(Ref<Font> font, const std::string &text);

    static Clay_Dimensions Clay_MeasureText(Clay_StringSlice text,
                                            Clay_TextElementConfig *config,
                                            void *userData);
};

} // namespace Pyxis
