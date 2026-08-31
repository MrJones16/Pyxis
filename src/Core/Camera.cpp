#include <Core/Camera.h>
namespace Pyxis {
Entity Camera::s_MainCameraEntity = Entity();
void Camera::SetMainCameraEntity(Entity e) {
    if (e.TryGetComponent<CameraComponent>() != nullptr &&
        e.TryGetComponent<TransformComponent>() != nullptr) {
        s_MainCameraEntity = e;
    }
}
Entity Camera::GetMainCameraEntity() { return s_MainCameraEntity; };
glm::vec2 Camera::GetMousePositionWorld() {
    if (s_MainCameraEntity.IsValid()) {
        auto camera = s_MainCameraEntity.TryGetComponent<CameraComponent>();
        auto transform =
            s_MainCameraEntity.TryGetComponent<TransformComponent>();
        if (camera && transform) {
            return camera->ProjectMouseNDC(Input::GetMousePositonNDC()) +
                   (glm::vec2)transform->GetWorldPosition();
        } else {
            PX_ERROR(
                "Tried getting mouse pos world when there is no valid main "
                "camera!");
            return Input::GetMousePositonNDC();
        }
    } else {
        PX_ERROR("Tried getting mouse pos world when there is no valid main "
                 "camera!");
        return Input::GetMousePositonNDC();
    }
}

} // namespace Pyxis
