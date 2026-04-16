#pragma once
#include <glm/glm.hpp>

namespace Pyxis {
typedef struct TransformComonent {
    glm::vec3 position;
    float rotation;
    glm::vec3 scale;

} TransformComponent;
} // namespace Pyxis
