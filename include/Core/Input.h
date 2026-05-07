#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <glm/glm.hpp>
namespace Pyxis {

typedef struct Keystate {
    bool pressed;
    bool repeat;
    bool justPressed;
} Keystate;

class Input {
  private:
  public:
    static void OnEvent(SDL_Event *event);

    static bool GetKeyDown(SDL_Scancode sdlScancode);
    static glm::vec2 GetMousePositon();
    static glm::vec2 GetMousePositonNDC();
    static bool GetMouseButtonDown(uint32_t mouseButton);
    static glm::vec2 GetMouseWheel();
};
} // namespace Pyxis
