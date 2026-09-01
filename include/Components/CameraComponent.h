#pragma once
#include <Core/Core.h>
#include <Core/Entity.h>
#include <glm/glm.hpp>
#include <nlohmann/detail/macro_scope.hpp>

namespace Pyxis {
// WIP
struct CameraComponent {

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CameraComponent, m_ProjectionType, m_Size,
                                   m_Near, m_Far, m_ProjectionMatrix);

    enum ProjectionType { Orthographic, Perspective };

  private:
    ProjectionType m_ProjectionType = Orthographic;

    // when ortho, this is width & height
    // when perspective, this is FOV & aspect
    glm::vec2 m_Size = {1920, 1080};
    float m_Near = 0.01;
    float m_Far = 1000;

    glm::mat4 m_ProjectionMatrix;
    glm::vec2 offsetToGrid = {0, 0};

  public:
    // Set the projection matrix to an orthographic one
    void SetOrthographicProjection(const glm::vec2 &size, float near = 0,
                                   float far = 1000);
    // Set the projection matrix to a perspective one
    void SetPerspectiveProjection(float FOV, float aspect, float near = 0.05f,
                                  float far = 1000);
    ProjectionType GetProjectionType();

    glm::mat4 GetViewProjectionMatrix(const glm::mat4 &transform);

    glm::vec2 ProjectMouseNDC(glm::vec2 mousePosNDC);

    glm::vec2 GetSize();
};
} // namespace Pyxis
