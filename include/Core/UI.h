#pragma once
#include <Components/UIComponent.h>
#include <Core/Timestep.h>
#include <UI/UIButtonModule.h>
#include <UI/UIModule.h>
#include <UI/UITextButtonModule.h>
#include <UI/UITextModule.h>

namespace Pyxis {

class UI {
  public:
    static void Init();
    static void OnWindowResize(const glm::vec2 &resolution);
    static void OnMouseWheelEvent(const glm::vec2 &mouseWheel);
    static void BeginUILayout();
    static Clay_RenderCommandArray EndUILayout(const Timestep &ts);
};
} // namespace Pyxis
