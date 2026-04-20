# Text Rendering System

The Pyxis engine includes a glyph-based text rendering system built on SDL3's TTF (True Type Font) support, integrated with the SDL3 GPU rendering backend and pipeline architecture.

## Architecture Overview

The text rendering system is designed around these core components:

### 1. **Glyph Atlas** (`GlyphAtlas` class)
- Manages a single GPU texture containing cached glyphs for a font
- Renders individual characters on-demand and packs them into the atlas texture
- Tracks UV coordinates for each glyph to enable efficient quad rendering
- Uses a simple row-based packing algorithm (left-to-right, top-to-bottom)

### 2. **Glyph Structure** (`Glyph` struct)
Contains metadata for each cached character:
- `atlasPosition` - Position in the atlas texture
- `size` - Width and height of the rendered glyph
- `bearing` - Offset from baseline (for proper vertical alignment)
- `advance` - Distance to advance cursor for next character
- `uvBounds` - Normalized UV coordinates (0.0-1.0) for sampling the atlas

### 3. **Text Vertex Format** (`TextVertex` struct)
Vertex data for text rendering:
```cpp
struct TextVertex {
    glm::vec3 position;  // Screen-space position (x, y, z-depth)
    glm::vec2 uv;        // UV coordinates in glyph atlas
    glm::vec4 color;     // RGBA color (0.0-1.0 range)
};
```

### 4. **Text Pipeline**
- Automatically created on first font load
- Dedicated shader pipeline optimized for text rendering
- Batches multiple text strings for efficient rendering
- Uses the atlas texture as sampler input

### 5. **Text Class**
Central API for text rendering:
- Manages font loading and glyph atlas creation
- Provides `QueueText()` for batching text to the pipeline
- Handles font lifecycle (load/unload)

## API Reference

### Font Management

#### Load a Font
```cpp
int fontID = Renderer::LoadFont("path/to/font.ttf", fontSize);
```
- **Parameters:**
  - `fontPath` (string): Path to the TTF font file
  - `fontSize` (uint32_t): Font size in pixels
- **Returns:** Font ID (int) for use in text rendering calls, or -1 on failure
- **Side Effect:** Creates a glyph atlas texture for this font

#### Unload a Font
```cpp
Renderer::UnloadFont(fontID);
```
- **Parameters:**
  - `fontID` (int): Font ID returned from `LoadFont()`
- **Effect:** Releases the glyph atlas texture and font resources

### Text Rendering

#### Queue Text for Rendering
```cpp
uint32_t vertexCount = Renderer::QueueText(fontID, "Hello World",
                                           glm::vec2(100.0f, 50.0f),
                                           glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
```
- **Parameters:**
  - `fontID` (int): Font ID from `LoadFont()`
  - `text` (string): Text string to render (supports ASCII)
  - `position` (glm::vec2): Screen-space position for text origin
  - `color` (glm::vec4): Text color in RGBA format (0.0f - 1.0f range)
- **Returns:** Number of vertices generated (6 per character for 2 triangles)
- **Important:** This queues vertices to the text pipeline. Call during `Renderer::BeginFrame()` / `Renderer::EndFrame()` block

#### Get Text Dimensions
```cpp
glm::ivec2 size = Renderer::GetTextSize(fontID, "Hello World");
```
- **Parameters:**
  - `fontID` (int): Font ID from `LoadFont()`
  - `text` (string): Text to measure
- **Returns:** `glm::ivec2` containing width and height in pixels
- **Use Case:** Useful for centering text or layout calculations

## Usage Example

```cpp
#include <Renderer/Renderer.h>

// Initialize (Text system is initialized automatically in Renderer::Init)
Renderer::Init("My App", glm::ivec2(1280, 720));

// Load a font
int myFont = Renderer::LoadFont("assets/fonts/arial.ttf", 32);

// In your render loop
Renderer::BeginFrame();

// Render some text
uint32_t vertexCount = Renderer::QueueText(
    myFont,
    "Score: 1000",
    glm::vec2(100.0f, 50.0f),
    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)  // White text
);

// Get text size for layout
glm::ivec2 textSize = Renderer::GetTextSize(myFont, "Score: 1000");
std::cout << "Text dimensions: " << textSize.x << "x" << textSize.y << std::endl;

// Queue more text
Renderer::QueueText(
    myFont,
    "FPS: 60",
    glm::vec2(100.0f, 100.0f),
    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green text
);

// Draw the text pipeline
Renderer::DrawPipeline(Text::GetTextPipeline());

Renderer::EndFrame();

// Cleanup
Renderer::UnloadFont(myFont);
Renderer::Shutdown();
```

## Implementation Details

### Glyph Atlas Packing

The `GlyphAtlas` uses a simple row-based packing algorithm:

1. Glyphs are packed left-to-right in rows
2. When a glyph doesn't fit horizontally, move to the next row
3. Row height is determined by the tallest glyph in that row
4. Small padding (1 pixel) is added between glyphs to avoid sampling artifacts

**Default Atlas Size:** 512x512 pixels
- Sufficient for most fonts and character sets
- Can be customized when creating atlases (future enhancement)

### Rendering Process

1. **On `QueueText()` call:**
   - For each character in the string:
     - Request glyph from atlas (renders if not cached)
     - Calculate screen-space quad position
     - Apply bearing offset for vertical alignment
     - Generate 6 vertices (2 triangles) with UV coordinates
     - Queue vertices to text pipeline

2. **On `DrawPipeline(textPipelineID)`:**
   - Pipeline maps transfer buffer
   - GPU uploads vertex data
   - Render pass executes with glyph atlas texture as sampler
   - Fragment shader samples atlas and applies color

### Character Support

Currently supports:
- **ASCII characters** (0x00-0x7F)
- **Extended ASCII** (0x80-0xFF) - with UTF-8 conversion
- **Unicode** - UTF-8 conversion implemented in `RenderGlyphToAtlas()`

## Performance Considerations

### Strengths
- **Glyph caching:** Each character is rendered only once per font
- **Texture atlas:** All glyphs in one texture reduces state changes
- **Batching:** Multiple text strings queued in single pipeline
- **GPU acceleration:** Rendering done entirely on GPU

### Current Limitations
1. **No dynamic atlas expansion:** If atlas fills up, new glyphs will fail to render
2. **No kerning:** Character spacing uses advance values only
3. **Single rendering mode:** Uses `SDL_TTF_RenderText_Blended` (high quality)
4. **No ligatures:** Complex text shaping not implemented

## Shader Requirements

The text rendering pipeline requires:
- **Vertex Shader** (`assets/shaders/text_vertex.hlsl`)
- **Fragment Shader** (`assets/shaders/text_fragment.hlsl`)

These shaders should:
- Accept `TextVertex` input (position, UV, color)
- Sample the glyph atlas texture
- Apply the color tint
- Support blending for transparency

## Future Enhancements

1. **Dynamic Atlas Expansion:** Automatically create new atlases when full
2. **Kerning Support:** Use SDL_TTF kerning metrics for better spacing
3. **Text Shaping:** Support complex scripts and ligatures
4. **Multiple Rendering Modes:** Fast, smooth, shaded, etc.
5. **SDF Rendering:** Signed distance field rendering for scalable text
6. **Text Metrics:** Line height, baseline, ascender/descender tracking
7. **Text Effects:** Shadows, outlines, gradients

## Error Handling

All functions include error checking:
- Font loading failures log SDL error messages
- Invalid font IDs return safe default values
- Atlas overflow is logged and glyphs are skipped
- Text system initialization is verified before use

## Dependencies

- **SDL3** - Core graphics library
- **SDL3_ttf** - TrueType font support
- **GLM** - Vector math library
- **SDL3 GPU** - GPU rendering backend
