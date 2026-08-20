#define CLAY_IMPLEMENTATION
#include <Renderer/clay.h>

#include "Renderer/Renderer.h"
#include <Core/Core.h>
#include <Core/Input.h>
#include <Renderer/UI.h>

void HandleClayErrors(Clay_ErrorData errorData) {
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

// Example measure text function
static Clay_Dimensions MeasureText(Clay_StringSlice text,
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

namespace Pyxis {

void UI::Init() {
    // Note : malloc is only used here as an example, any allocator that
    // provides a pointer to addressable memory of at least totalMemorySize will
    // work
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
        totalMemorySize, malloc(totalMemorySize));

    auto size = Renderer::GetResolution();
    Clay_Initialize(arena, (Clay_Dimensions){size.x, size.y},
                    (Clay_ErrorHandler){HandleClayErrors});

    Clay_SetMeasureTextFunction(&Text::Clay_MeasureText, nullptr);
}

void UI::OnWindowResize(const glm::vec2 &resolution) {
    // clay has its own check if it's the same as well
    Clay_SetLayoutDimensions((Clay_Dimensions){resolution.x, resolution.y});
}

void UI::OnMouseWheelEvent(const glm::vec2 &mouseWheel) {

    PX_TRACE("Scrolled mousewheel: {}", mouseWheel);
    Clay_UpdateScrollContainers(
        true, (Clay_Vector2){mouseWheel.x, mouseWheel.y}, 0.01f);
}

void UI::BeginUILayout() {

    // Update internal pointer position for handling mouseover / click
    // / touch events - needed for scrolling & debug tools
    glm::vec2 mousePosition = Input::GetMousePositon();
    Clay_SetPointerState((Clay_Vector2){mousePosition.x, mousePosition.y},
                         Input::GetMouseButtonDown(SDL_BUTTON_LEFT));

    // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
    Clay_BeginLayout();
}

Clay_RenderCommandArray UI::EndUILayout(const Timestep &ts) {
    return Clay_EndLayout(ts.GetSeconds());
}

} // namespace Pyxis
