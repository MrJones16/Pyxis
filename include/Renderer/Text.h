#pragma once

#include <Renderer/Renderer.h>
#include <Renderer/Texture.h>
#include <Renderer/clay.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glm/glm.hpp>
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
class Font {
  public:
    // Creates a Font to be referenced by a renderer implementation
    Font(const std::string fontPath, uint32_t fontSize);
    static Ref<Font> LoadFont(const std::string fontAssetPath,
                              uint32_t fontSize);
    ~Font();

    // Get or create a glyph in the atlas
    const Glyph *GetGlyph(uint32_t codePoint);

    // Get the texture for this atlas
    Ref<Texture> GetTexture() const { return m_Texture; }

    // Get font metrics
    int GetLineHeight() const { return m_LineHeight; }
    int GetBaseline() const { return m_Baseline; }

  protected:
    TTF_Font *m_Font;
    uint32_t m_FontSize;
    Ref<Texture> m_Texture;

    // Cached glyphs that exist in the atlas
    std::unordered_map<uint32_t, Glyph> m_Glyphs;

    // Font metrics
    int m_LineHeight;
    int m_Baseline;

    // Atlas texture and dimensions
    SDL_Surface *m_AtlasSurface;
    glm::ivec2 m_AtlasSize;

    // Current packing position for new glyphs
    uint32_t m_AtlasX;
    uint32_t m_AtlasY;
    uint32_t m_AtlasRowHeight;

    // Helper to render a codepoint into the atlas. Uses the pack function
    // below.
    void AddCodepoint(uint32_t codepoint, glm::vec2 bearing, int advance);

    // Helper to pack a glyph surface into the atlas surface
    bool PackGlyphSurface(SDL_Surface *atlasSurface, SDL_Surface *glyphSurface,
                          uint32_t codepoint, glm::vec2 bearing, int advance);
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

    // Initialize text system with GPU device
    static bool Init(SDL_GPUDevice *device);
    static void Shutdown();

    // Font management
    // Returns font ID for use in rendering calls
    static int LoadFont(const std::string &fontPath, uint32_t fontSize);
    static void UnloadFont(int fontID);

    // gets the texture of the font atlas, needed when drawing the text.
    static Ref<Texture> GetFontTexture(int fontID);

    // for debugging if needed later
    static Font *GetFont(int fontID);

    struct GlyphCommand {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec4 uvBounds;
    };

    static std::vector<GlyphCommand>
    DrawText(int fontID, const glm::vec2 &position, const std::string &text,
             const glm::vec4 &color = {1, 1, 1, 1},
             const glm::vec2 &scale = {1, 1});

    // Get text dimensions without rendering
    static glm::ivec2 GetTextSize(int fontID, const std::string &text);

    static Clay_Dimensions Clay_MeasureText(Clay_StringSlice text,
                                            Clay_TextElementConfig *config,
                                            void *userData);

  private:
    struct FontData {
        Font *atlas = nullptr;
        TTF_Font *font = nullptr;
        uint32_t fontSize = 0;
    };

    static SDL_GPUDevice *s_GPUDevice;
    static std::unordered_map<int, FontData> s_Fonts;
    static int s_NextFontID;
};

} // namespace Pyxis
