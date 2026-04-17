#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entt.hpp>
#include <vector>

// this header defines the global entity registry. I'd rather have a global one
// so that future code doesn't have to be like
//
//  GetScene().Instantiate yada yada, and can just create/destroy entities from
//  anywhere. aka, fuck it! global it is
namespace Pyxis {

// I redefined the term "Entity", so that you can have an entity handle, and do
// convenient things with it, including incorporating easy access to Transform
// properties.
class Entity {
  private:
    static entt::registry s_Registry;

  public:
    // global registry functions
    inline static void ClearRegistry() { s_Registry.clear(); }
    static Entity Instantiate();
    static void Destroy(Entity entity);

    template <typename Component, typename... Args>
    static void AddComponent(Entity entity, Args &&...args) {
        s_Registry.emplace<Component>(entity, std::forward<Args>(args)...);
    }
    template <typename Component> static void RemoveComponent(Entity entity) {
        s_Registry.remove<Component>(entity);
    }

  public:
    // the only member variable, everything else is just functions around this
    // id and the registry!
    entt::entity m_entt;

    Entity(entt::entity entity) : m_entt(entity) {};

    // overload conversion to entt
    operator entt::entity() const { return m_entt; }

    bool IsValid() { return s_Registry.valid(m_entt); }

    template <typename Component, typename... Args>
    void AddComponent(Args &&...args) const {
        s_Registry.emplace<Component>(m_entt, std::forward<Args>(args)...);
    }

    template <typename Component> void RemoveComponent() const {
        s_Registry.remove<Component>(m_entt);
    }

    void SetParent(Entity entity);
    Entity GetParent();

    // returns a copy of the children vector
    std::vector<Entity> GetChildrenCopy();

    // returns pointer to children vector, or null
    std::vector<Entity> *GetChildren();

    void AddChild(Entity entity);
    void RemoveChild(Entity entity);
};

} // namespace Pyxis
