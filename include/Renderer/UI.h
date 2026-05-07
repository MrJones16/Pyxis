#pragma once
#include <Core/Timestep.h>
#include <Renderer/Text.h>

#define CLAY_IMPLEMENTATION
#include <Renderer/clay.h>

inline void HandleClayErrors(Clay_ErrorData errorData) {
    // See the Clay_ErrorData struct for more information
    PX_ERROR("CLAY ERR: {}", errorData.errorText.chars);
    switch (errorData.errorType) {
    case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_FUNCTION_NOT_PROVIDED:
    case CLAY_ERROR_TYPE_ARENA_CAPACITY_EXCEEDED:
    case CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED:
    case CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED:
    case CLAY_ERROR_TYPE_DUPLICATE_ID:
    case CLAY_ERROR_TYPE_FLOATING_CONTAINER_PARENT_NOT_FOUND:
    case CLAY_ERROR_TYPE_PERCENTAGE_OVER_1:
    case CLAY_ERROR_TYPE_INTERNAL_ERROR:
    case CLAY_ERROR_TYPE_UNBALANCED_OPEN_CLOSE:
    case CLAY_ERROR_TYPE_HASH_MAP_CAPACITY_EXCEEDED:
        break;
    }
}

namespace Pyxis {
// Example measure text function
static inline Clay_Dimensions MeasureText(Clay_StringSlice text,
                                          Clay_TextElementConfig *config,
                                          uintptr_t userData) {
    // Clay_TextElementConfig contains members such as fontId, fontSize,
    // letterSpacing etc Note: Clay_String->chars is not guaranteed to be null
    // terminated
    return (Clay_Dimensions){
        .width =
            (float)(text.length *
                    config->fontSize), // <- this will only work for monospace
                                       // fonts, see the renderers/ directory
                                       // for more advanced text measurement
        .height = (float)config->fontSize};
}

class UI {
  private:
  public:
    static void Init();
    static void OnWindowResize(const glm::vec2 &resolution);
    static void OnMouseWheelEvent(const glm::vec2 &mouseWheel);
    static void BeginUILayout();
    static Clay_RenderCommandArray EndUILayout(const Timestep &ts);
};
} // namespace Pyxis
