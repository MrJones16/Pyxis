#include <Components/TransformComponent.h>
#include <Core/Entity.h>
#include <entt/entity/fwd.hpp>

namespace Pyxis {

entt::registry Entity::s_Registry = entt::registry();

void Entity::SetParent(Entity entity) {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(m_entt);
    if (transform != nullptr) {
        transform->parent = entity;
    }
}

Entity Entity::GetParent() {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(m_entt);
    if (transform != nullptr) {
        return transform->parent;
    } else {
        return (entt::entity)entt::null;
    }
}

std::vector<Entity> Entity::GetChildrenCopy() {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(m_entt);
    if (transform != nullptr) {
        return transform->children;
    }
    return std::vector<Entity>();
}

std::vector<Entity> *Entity::GetChildren() {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(m_entt);
    if (transform != nullptr) {
        return &transform->children;
    }
    return nullptr;
}

void Entity::AddChild(Entity entity) {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(m_entt);
    if (transform != nullptr) {
        transform->children.push_back(entity);
    }
}

Entity Entity::Instantiate() {
    Entity e = s_Registry.create();
    s_Registry.emplace<TransformComponent>(e);
    return e;
}
void Entity::Destroy(Entity entity) {
    TransformComonent *transform =
        s_Registry.try_get<TransformComponent>(entity);
    if (transform != nullptr) {
        std::vector<Entity> *children = entity.GetChildren();
        for (Entity e : *children) {
            Destroy(e);
        }
    }

    s_Registry.destroy(entity);
}

} // namespace Pyxis
