#include <Core/Core.h>
#include <Core/EntityArchive.h>
#include <algorithm>
#include <unordered_set>

namespace Pyxis {

std::vector<entt::entity> EntityArchive::CollectEntities(
    entt::registry &registry,
    const std::function<bool(entt::entity)> &filter) const {
    std::vector<entt::entity> all, entities;
    for (const RegisteredComponent &c : m_Components)
        c.appendEntities(registry, all);
    std::unordered_set<entt::entity> seen;
    for (entt::entity entity : all)
        if (seen.insert(entity).second && (!filter || filter(entity)))
            entities.push_back(entity);
    return entities;
}

nlohmann::json
EntityArchive::Save(entt::registry &registry,
                    const std::vector<entt::entity> &entities) const {
    EntitySaveContext context;
    context.entities = entities;
    for (uint32_t i = 0; i < entities.size(); i++)
        context.archiveIds[entities[i]] = i;

    nlohmann::json saved = nlohmann::json::array();
    for (entt::entity entity : entities) {
        nlohmann::json components = nlohmann::json::object();
        for (const RegisteredComponent &c : m_Components)
            if (c.has(registry, entity))
                c.save(context, registry, entity, components[c.name]);
        saved.push_back({{"components", std::move(components)}});
    }
    return saved;
}

std::vector<entt::entity> EntityArchive::Load(
    entt::registry &registry, const nlohmann::json &entities,
    const std::function<entt::entity(size_t archiveId)> &getEntity) const {
    EntityLoadContext context;
    context.entities.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); i++)
        context.entities.push_back(getEntity(i));

    for (size_t i = 0; i < entities.size(); i++) {
        const nlohmann::json &components = entities[i].at("components");
        for (const RegisteredComponent &c : m_Components) {
            auto it = components.find(c.name);
            if (it != components.end())
                c.load(context, registry, context.entities[i], *it);
        }
        for (auto &[name, value] : components.items())
            if (std::none_of(m_Components.begin(), m_Components.end(),
                             [&](const RegisteredComponent &c) {
                                 return c.name == name;
                             }))
                PX_WARN("EntityArchive: unknown component '{}' skipped", name);
    }
    return context.entities;
}

} // namespace Pyxis
