#pragma once
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

// thanks to Thomas Trocha for the code:
// https://thomas.trocha.com/blog/entt-de-serialization-with-nlohmann-json/

// entity archive example:
// void test()
//{
//     entt::registry reg;
//     auto e1 = reg.create();
//     Tower t{"Tower", 1895, 18.95f};
//     t.name = "hansi";
//     Tower tw1 = reg.emplace<Tower>(e1, std::string("tower1"), 1895, 18.95f);
//     reg.emplace<Transform>(e1, 1.0f, 1.0f);
//
//     auto e2 = reg.create();
//     Tower tw2 = reg.emplace<Tower>(e2, "tower2", 18, 0.95f);
//     reg.emplace<Transform>(e2, 5.0f, 5.0f);
//
//     auto w1 = reg.create();
//     Walker &w = reg.emplace<Walker>(w1, 1, 0.5f);
//     reg.emplace<Transform>(w1, 100.0f, 100.0f);
//
//     JSONEntityOutputArchive json_archive;
//     entt::basic_snapshot snapshot(reg);
//     snapshot.entities(json_archive)
//         .component<Tower, Walker, Transform>(json_archive);
//     json_archive.Close();
//     std::string json_output = json_archive.AsString();
//     printf("json:%s", json_output.c_str());
//
//     JSONEntityInputArchive json_in(json_output);
//     entt::registry reg2;
//     entt::basic_snapshot_loader loader(reg2);
//     loader.entities(json_in)
//         .component<Tower, Walker, Transform>(json_in);
//
//     auto _e1 = entt::entity(0);
//     auto _e2 = entt::entity(1);
//     auto [tower, transform] = reg2.get<Tower, Transform>(e1);
//     auto [tower2, transform2] = reg2.get<Tower, Transform>(e2);
// }

namespace Pyxis {

// nlohmann-json based entt-(de)serialization
class JSONEntityOutputArchive {
  public:
    JSONEntityOutputArchive() { root = nlohmann::json::array(); };

    // new element for serialization. giving you the amount of elements that is
    // going to be pumped into operator()(entt::entity ent) or
    // operator()(entt::entity ent, const T &t)
    void operator()(std::underlying_type_t<entt::entity> size) {
        int a = 0;
        if (!current.empty()) {
            root.push_back(current);
        }
        current = nlohmann::json::array();
        current.push_back(
            size); // first element of each array keeps the amount of elements.
    }

    // persist entity ids
    void operator()(entt::entity entity) {
        // Here it is assumed that no custom entt-type is chosen
        current.push_back((uint32_t)entity);
    }

    // persist components
    // ent is the entity and t a component that is attached to it
    // in json we first push the entity-id and then convert the component
    // to json just by assigning:  'nlohmann:json json=t'
    // For this to work all used component musst have following in its body:
    // NLOHMANN_DEFINE_TYPE_INTRUSIVE([component_name], fields,....)
    // e.g.
    // struct Transform {
    //     float x;
    //     float y;
    //
    //     NLOHMANN_DEFINE_TYPE_INTRUSIVE(Transform, x,y)
    // };
    //
    template <typename T> void operator()(entt::entity ent, const T &t) {
        current.push_back(
            (uint32_t)ent); // persist the entity id of the following component

        // auto factory = entt::type_id<T>();
        // std::string component_name = std::string(factory.name());
        // current.push_back(component_name);

        nlohmann::json json = t;
        current.push_back(json);
    }

    void Close() {
        if (!current.empty()) {
            root.push_back(current);
        }
    }

    // create a json as string
    const std::string AsString() {
        std::string output = root.dump();
        return output;
    }

    // create bson-data
    const std::vector<uint8_t> AsBson() {
        std::vector<std::uint8_t> as_bson = nlohmann::json::to_bson(root);
        return as_bson;
    }

  private:
    nlohmann::json root;
    nlohmann::json current;
};

class JSONEntityInputArchive {
  private:
    nlohmann::json root;
    nlohmann::json current;

    int root_idx = -1;
    int current_idx = 0;

  public:
    JSONEntityInputArchive(const std::string &json_string) {
        root = nlohmann::json::parse(json_string);
    };

    ~JSONEntityInputArchive() {}

    void next_root() {
        root_idx++;
        if (root_idx >= root.size()) {
            // ERROR
            return;
        }
        current = root[root_idx];
        current_idx = 0;
    }

    void operator()(std::underlying_type_t<entt::entity> &s) {
        next_root();
        int size = current[0].get<int>();
        current_idx++;
        s = (std::underlying_type_t<entt::entity>)size; // pass amount to entt
    }

    void operator()(entt::entity &entity) {
        uint32_t ent = current[current_idx].get<uint32_t>();
        entity = entt::entity(ent);
        current_idx++;
    }

    template <typename T> void operator()(entt::entity &ent, T &t) {
        nlohmann::json component_data = current[current_idx * 2];

        auto comp = component_data.get<T>();
        t = comp;

        uint32_t _ent = current[current_idx * 2 - 1];
        ent = entt::entity(_ent); // last element is the entity-id
        current_idx++;
    }
};
} // namespace Pyxis
