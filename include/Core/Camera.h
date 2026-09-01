#pragma once
#include <Components/CameraComponent.h>
#include <Components/TransformComponent.h>
#include <Core/Input.h>

namespace Pyxis {
class Camera {
    static Entity s_MainCameraEntity;
    static glm::vec3
        s_OffsetToGrid; // vector pointing from grid position to actual position

  public:
    static void SetMainCameraEntity(Entity e);
    static Entity GetMainCameraEntity();
    static glm::vec2 GetMousePositionWorld();

    static glm::mat4 GetViewProjectionMatrixSnapped();
    static glm::mat4 GetViewProjectionMatrix();

    static glm::vec2 GetSize();
    static glm::vec3 GetOffset();

  private:
    static bool AssertValidCamera();
};
} // namespace Pyxis
