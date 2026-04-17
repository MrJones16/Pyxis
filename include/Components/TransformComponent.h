#pragma once
#include <Core/Entity.h>
#include <glm/glm.hpp>

namespace Pyxis {

typedef struct TransformComonent {

    glm::mat4 transform;

    Entity parent = entt::entity(entt::null);
    std::vector<Entity> children = std::vector<Entity>();

} TransformComponent;
} // namespace Pyxis
