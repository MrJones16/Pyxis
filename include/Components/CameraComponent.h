#pragma once
#include <Core/Entity.h>
#include <glm/glm.hpp>

namespace Pyxis {
// WIP
typedef struct CameraComponent {

    enum ProjectionType { Orthographic, Perspective };

  private:
    ProjectionType m_ProjectionType;

    // when ortho, this is width & height
    // when perspective, this is FOV & aspect
    glm::vec2 m_Size;
    float m_Near;
    float m_Far;

    glm::mat4 m_ProjectionMatrix;

    glm::vec3 offsetToGrid;

  public:
    void SetOrthographicProjection(const glm::vec2 &size, float near = 0,
                                   float far = 1000);
    void SetPerspectiveProjection(float FOV, float aspect, float near = 0.05f,
                                  float far = 1000);
    ProjectionType GetProjectionType();

    glm::mat4 GetViewProjectionMatrix(const glm::mat4 &transform);

} CameraComponent;
} // namespace Pyxis
