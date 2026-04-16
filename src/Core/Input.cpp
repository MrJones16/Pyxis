#include <Core/Input.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>

namespace Pyxis {
bool Input::GetKeyDown(SDL_Scancode sdlScancode) {
    return SDL_GetKeyboardState(nullptr)[sdlScancode];
}
glm::vec2 Input::GetMousePositon() {
    glm::vec2 mousePos;
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    return mousePos;
}

bool Input::GetMouseButtonDown(uint32_t mouseButton) {
    return SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(mouseButton);
}

} // namespace Pyxis
