#include "Renderer/Renderer.h"
#include <Core/Core.h>
#include <Core/Input.h>
#include <Renderer/UI.h>

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
