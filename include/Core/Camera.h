#pragma once
#include <Components/CameraComponent.h>
#include <Components/TransformComponent.h>
#include <Core/Input.h>

namespace Pyxis {
class Camera {
    static Entity s_MainCameraEntity;

  public:
    static void SetMainCameraEntity(Entity e);
    static Entity GetMainCameraEntity();
    static glm::vec2 GetMousePositionWorld();
};
} // namespace Pyxis
