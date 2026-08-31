#include <Components/CameraComponent.h>
#include <Components/TransformComponent.h>
#include <glm/gtc/matrix_transform.hpp>
namespace Pyxis {

void CameraComponent::SetOrthographicProjection(const glm::vec2 &size,
                                                float near, float far) {
    m_Size = size;
    m_Near = near;
    m_Far = far;
    m_ProjectionMatrix =
        glm::ortho(-m_Size.x / 2, m_Size.x / 2, -(m_Size.y) / 2, (m_Size.y) / 2,
                   m_Near, m_Far);
    m_ProjectionType = Orthographic;
}
void CameraComponent::SetPerspectiveProjection(float FOV, float aspect,
                                               float near, float far) {
    m_Size = {FOV, aspect};
    m_Near = near;
    m_Far = far;
    m_ProjectionMatrix =
        glm::perspective(glm::radians(FOV), aspect, m_Near, m_Far);
    m_ProjectionType = Perspective;
}

CameraComponent::ProjectionType CameraComponent::GetProjectionType() {
    return m_ProjectionType;
}

glm::mat4 CameraComponent::GetViewProjectionMatrix(const glm::mat4 &transform) {

    return m_ProjectionMatrix * glm::inverse(transform);
}

glm::vec2 CameraComponent::ProjectMouseNDC(glm::vec2 mousePosNDC) {
    // TODO: setup pixel snapping
    return ((m_Size / 2.0f) * mousePosNDC);
}

} // namespace Pyxis
