#pragma once
#include <Components/UIComponent.h>
#include <Core/Timestep.h>
#include <Renderer/UI/UIButtonModule.h>
#include <Renderer/UI/UIContainerModule.h>
#include <Renderer/UI/UIModule.h>
#include <Renderer/UI/UITextButtonModule.h>
#include <Renderer/UI/UITextModule.h>

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
