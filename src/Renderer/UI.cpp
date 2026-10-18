#define CLAY_IMPLEMENTATION
#include <Renderer/clay.h>

#include <Core/Core.h>
#include <Core/Input.h>
#include <Core/UI.h>
#include <Renderer/Text.h>

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

namespace Pyxis {

void UI::Init() {
    // Clay's arena: allocated once here and used for the whole run (never
    // freed)
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
