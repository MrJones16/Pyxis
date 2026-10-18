#pragma once
#include <entt/entt.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pyxis {

// Saving: maps the saved entities to archive ids (their index in the save),
// so components can store references to other saved entities.
struct EntitySaveContext {
    // the saved entities, in save order (archive id = index)
    std::vector<entt::entity> entities;
    std::unordered_map<entt::entity, uint32_t> archiveIds;

    // archive id of entity, -1 if it isn't part of this save
    int64_t ArchiveIdOf(entt::entity entity) const {
        auto it = archiveIds.find(entity);
        return it != archiveIds.end() ? (int64_t)it->second : -1;
    }
};

// Loading: archive id -> the entity created (or given) for it.
struct EntityLoadContext {
    std::vector<entt::entity> entities;

    // the entity for an archive id (entt::null for -1 / unknown ids)
    entt::entity EntityOf(int64_t archiveId) const {
        return archiveId >= 0 && archiveId < (int64_t)entities.size()
                   ? entities[archiveId]
                   : entt::null;
    }
};

// Saves / loads entities with their components, component by component,
// as nlohmann json (any binary json format can be used for storage).
//
// Every component type is registered once with a name (its key in the save):
//
//   archive.RegisterComponent<HealthComponent>("Health");
//
// uses the component's to_json / from_json (found by ADL). A component that
// needs more (entity references, creating objects in other systems, ...)
// registers custom save / load functions that get the save / load context:
// store references with EntitySaveContext::ArchiveIdOf and resolve them
// with EntityLoadContext::EntityOf.
//
// Save format: [ {"components": {"<name>": <component json>, ...}}, ... ],
// one object per entity, in the given order.
//
// Load: an entity is obtained for every saved entity first (so references
// can be resolved; usually freshly created, but the caller decides, e.g. to
// reuse an existing entity), then each entity's components are loaded in
// registration order (so a component can rely on components registered
// before it, on the same or on earlier entities), entity by entity in save
// order. Components are emplaced in that order, so the component storages
// get the saved order.
//
// Fixed order: CollectEntities lists entities by each registered
// component's storage in packed (insertion) order. Saving in that order and
// loading again recreates the storages in the same order, so systems that
// iterate them keep their order across a save / load (needed for a
// deterministic simulation), and saving the same state twice gives the same
// data.
//
// (EnTT's snapshot / continuous_loader is not used: it loads one storage at
// a time into default-constructed, moved-in components without a context,
// which doesn't fit components that must create objects in other systems
// while being loaded (Box2D bodies, chunk colliders) or that must already
// sit on their entity; entity references in members still need a manual
// pass; and the loader's remapping state is shared by everything it loads.)
class EntityArchive {
  public:
    using SaveFunction =
        std::function<void(const EntitySaveContext &, entt::registry &,
                           entt::entity, nlohmann::json &)>;
    using LoadFunction =
        std::function<void(const EntityLoadContext &, entt::registry &,
                           entt::entity, const nlohmann::json &)>;

    // registers a component saved / loaded with its to_json / from_json
    template <typename Component>
    void RegisterComponent(const std::string &name) {
        RegisterComponent<Component>(
            name,
            [](const EntitySaveContext &, entt::registry &registry,
               entt::entity entity, nlohmann::json &j) {
                j = registry.get<Component>(entity);
            },
            [](const EntityLoadContext &, entt::registry &registry,
               entt::entity entity, const nlohmann::json &j) {
                j.get_to(registry.emplace_or_replace<Component>(entity));
            });
    }

    // registers a component with custom save / load functions (load must
    // add the component to the entity)
    template <typename Component>
    void RegisterComponent(const std::string &name, SaveFunction save,
                           LoadFunction load) {
        m_Components.push_back(
            {name,
             [](entt::registry &registry, entt::entity entity) {
                 return registry.all_of<Component>(entity);
             },
             [](entt::registry &registry, std::vector<entt::entity> &out) {
                 // packed (insertion) order: EnTT's own iterators go
                 // backwards
                 auto &storage = registry.storage<Component>();
                 out.insert(out.end(), storage.data(),
                            storage.data() + storage.size());
             },
             std::move(save), std::move(load)});
    }

    // Every entity with at least one registered component for which filter
    // returns true (no filter: all), in the fixed order described above.
    std::vector<entt::entity> CollectEntities(
        entt::registry &registry,
        const std::function<bool(entt::entity)> &filter = nullptr) const;

    // saves entities (in this order) with their registered components
    nlohmann::json Save(entt::registry &registry,
                        const std::vector<entt::entity> &entities) const;

    // Loads entities saved by Save. getEntity(archiveId) returns the entity
    // to load each saved entity into (called once per entity, in order,
    // before any component is loaded). Returns the entities in save order.
    // Throws json / std exceptions on bad data (the entities obtained so far
    // are reported through getEntity, the caller cleans them up).
    std::vector<entt::entity>
    Load(entt::registry &registry, const nlohmann::json &entities,
         const std::function<entt::entity(size_t archiveId)> &getEntity)
        const;

  private:
    struct RegisteredComponent {
        std::string name;
        std::function<bool(entt::registry &, entt::entity)> has;
        std::function<void(entt::registry &, std::vector<entt::entity> &)>
            appendEntities;
        SaveFunction save;
        LoadFunction load;
    };
    std::vector<RegisteredComponent> m_Components;
};

} // namespace Pyxis
