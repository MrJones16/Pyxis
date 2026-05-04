#pragma once

#include <Core/Core.h>
#include <Renderer/Material.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pyxis {

// Forward declaration
class Renderer;

// Represents a single glyph in the atlas
struct Glyph {
    glm::ivec2 atlasPosition; // Position in the atlas texture
    glm::ivec2 size;          // Width and height of the glyph
    glm::ivec2 bearing;       // Offset from baseline (x, y)
    int advance;              // Advance to next character
    glm::vec4
        uvBounds; // Normalized UV coordinates (min_x, min_y, max_x, max_y)
};

// Manages a texture atlas containing glyphs for a single font
class GlyphAtlas {
  public:
    GlyphAtlas(SDL_GPUDevice *device, SDL_GPUCommandBuffer *commandBuffer,
               TTF_Font *font, uint32_t fontSize);
    ~GlyphAtlas();

    // Get or create a glyph in the atlas
    const Glyph *GetGlyph(uint32_t codePoint);

    Ref<Material> GetMaterial() const { return m_Material; }
    // Get the texture for this atlas
    Ref<Texture> GetTexture() const { return m_Texture; }

    // Get font metrics
    int GetLineHeight() const { return m_LineHeight; }
    int GetBaseline() const { return m_Baseline; }

  private:
    SDL_GPUDevice *m_Device;
    TTF_Font *m_Font;
    uint32_t m_FontSize;

    // Atlas texture and dimensions
    Ref<Material> m_Material;
    Ref<Texture> m_Texture;
    glm::ivec2 m_AtlasSize;

    // Current packing position for new glyphs
    uint32_t m_CurrentX;
    uint32_t m_CurrentY;
    uint32_t m_RowHeight;

    // Font metrics
    int m_LineHeight;
    int m_Baseline;

    // Cached glyphs
    std::unordered_map<uint32_t, Glyph> m_Glyphs;

    // Helper to render a glyph and add it to the atlas
    const Glyph *RenderGlyphToAtlas(uint32_t codepoint);

    // Helper to pack a glyph surface into the atlas
    bool PackGlyphSurface(SDL_Surface *atlasSurface, SDL_Surface *glyphSurface,
                          uint32_t codepoint, glm::ivec2 bearing, int advance);
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
    static GlyphAtlas *GetGlyphAtlas(int fontID);

    // Get the text rendering pipeline
    // This is pipeline is made on init
    static int GetTextPipeline() { return s_TextPipelineID; }

    // Queue text for rendering
    // Position is in screen space, color is RGBA (0.0-1.0 range)
    // Returns the number of vertices queued
    static uint32_t QueueText(int fontID, const std::string &text,
                              const glm::vec2 &position, const glm::vec4 &color,
                              const glm::vec2 &scale = {1, 1});

    // Get text dimensions without rendering
    static glm::ivec2 GetTextSize(int fontID, const std::string &text);

  private:
    struct FontData {
        GlyphAtlas *atlas = nullptr;
        TTF_Font *font = nullptr;
        uint32_t fontSize = 0;
    };

    static SDL_GPUDevice *s_GPUDevice;
    static std::unordered_map<int, FontData> s_Fonts;
    static int s_NextFontID;
    static int s_TextPipelineID;
};

} // namespace Pyxis
