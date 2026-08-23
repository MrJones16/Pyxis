#pragma once
#include <Core/Core.h>
#include <Core/Entity.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
// #include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Pyxis {

struct TransformComponent {

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransformComponent, transform, parent,
                                   children);

    glm::mat4 transform = glm::mat4(1);

    Entity parent = entt::entity(entt::null);
    std::vector<Entity> children = std::vector<Entity>();

  public:
    inline glm::mat4 GetWorldTransform() {
        if (parent == entt::entity(entt::null)) {
            return transform;
        } else {
            TransformComponent *parentTransform =
                parent.TryGetComponent<TransformComponent>();
            if (parentTransform != nullptr) {
                return parentTransform->GetWorldTransform() * transform;
            } else {
                return transform;
            }
        }
    }

    inline glm::vec3 GetLocalPosition() {
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        if (glm::decompose(transform, scale, rotation, translation, skew,
                           perspective)) {
            return translation;
        } else {
            PX_ASSERT(false, "Failed to decompose matrix!");
            return {1, 1, 1};
        }
    }
    inline glm::vec3 GetWorldPosition() {
        glm::mat4 T = transform;
        if (parent != entt::entity(entt::null)) {
            TransformComponent *parentTransform =
                parent.TryGetComponent<TransformComponent>();
            if (parentTransform != nullptr) {
                T = parentTransform->GetWorldTransform() * transform;
            }
        }
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        if (glm::decompose(T, scale, rotation, translation, skew,
                           perspective)) {
            return translation;
        } else {
            PX_ASSERT(false, "Failed to decompose matrix!");
            return {1, 1, 1};
        }
    }
};
} // namespace Pyxis
