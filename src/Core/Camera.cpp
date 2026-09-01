#include <Core/Camera.h>
namespace Pyxis {

Entity Camera::s_MainCameraEntity = Entity();
glm::vec3 Camera::s_OffsetToGrid = {0, 0, 0};

void Camera::SetMainCameraEntity(Entity e) {
    if (e.TryGetComponent<CameraComponent>() != nullptr &&
        e.TryGetComponent<TransformComponent>() != nullptr) {
        s_MainCameraEntity = e;
    }
}
Entity Camera::GetMainCameraEntity() { return s_MainCameraEntity; };
glm::vec2 Camera::GetMousePositionWorld() {

    if (AssertValidCamera()) {
        auto camera = s_MainCameraEntity.GetComponent<CameraComponent>();
        auto transform = s_MainCameraEntity.GetComponent<TransformComponent>();
        return camera.ProjectMouseNDC(Input::GetMousePositonNDC()) +
               (glm::vec2)transform.GetWorldPosition();
    } else {
        PX_ERROR("Tried getting mouse pos world when there is no valid main "
                 "camera!");
        return Input::GetMousePositonNDC();
    }
}

glm::mat4 Camera::GetViewProjectionMatrixSnapped() {
    if (AssertValidCamera()) {
        auto transform = s_MainCameraEntity.GetComponent<TransformComponent>();
        auto camera = s_MainCameraEntity.GetComponent<CameraComponent>();
        auto worldPos = transform.GetWorldPosition();
        glm::vec3 worldPosSnapped = glm::floor(worldPos);
        s_OffsetToGrid = worldPos - worldPosSnapped;
        return camera.GetViewProjectionMatrix(
            glm::translate(transform.GetWorldTransform(), -s_OffsetToGrid));
    } else {
        PX_ERROR("No valid main camera set!");
        return {};
    }
}
glm::mat4 Camera::GetViewProjectionMatrix() {
    if (AssertValidCamera()) {
        auto transform = s_MainCameraEntity.GetComponent<TransformComponent>();
        auto camera = s_MainCameraEntity.GetComponent<CameraComponent>();
        s_OffsetToGrid = {0, 0, 0};
        return camera.GetViewProjectionMatrix(transform.GetWorldTransform());
    } else {
        PX_ERROR("No valid main camera set!");
        return {};
    }
}

bool Camera::AssertValidCamera() {
    if (!s_MainCameraEntity.IsValid())
        return false;
    if (s_MainCameraEntity.TryGetComponent<TransformComponent>() == nullptr ||
        s_MainCameraEntity.TryGetComponent<CameraComponent>() == nullptr)
        return false;
    return true;
}

glm::vec2 Camera::GetSize() {
    if (!AssertValidCamera()) {
        PX_ERROR("No valid main camera to get the size from!");
        return {0, 0};
    };
    return s_MainCameraEntity.GetComponent<CameraComponent>().GetSize();
}

glm::vec3 Camera::GetOffset() { return s_OffsetToGrid; }

} // namespace Pyxis
