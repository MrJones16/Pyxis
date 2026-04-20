#pragma once
#include <glm/glm.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace Pyxis {
// core macros and such!

// Colors for std::cout!
// found at:
// https://stackoverflow.com/questions/9158150/colored-output-in-c/9158263
#define RESET "\033[0m"
#define WHITE "\033[37m"
#define BLACK "\033[30m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define BOLDWHITE "\033[1m\033[37m"
#define BOLDBLACK "\033[1m\033[30m"
#define BOLDRED "\033[1m\033[31m"
#define BOLDYELLOW "\033[1m\033[33m"
#define BOLDGREEN "\033[1m\033[32m"
#define BOLDBLUE "\033[1m\033[34m"
#define BOLDCYAN "\033[1m\033[36m"
#define BOLDMAGENTA "\033[1m\033[35m"

// nerdfont UTF
#define PX_ICON_STEPIN "\uf444"
#define PX_ICON_STEPOUT "\uf4c3"
#define PX_ICON_INDENT "\uebf9"
#define PX_ICON_SUCCESS "\uf05d"
#define PX_ICON_WARN "\uea6c"
#define PX_ICON_FAILURE "\uf52f"
#define PX_ICON_SKULL "\uee15"

#ifdef PX_DEBUG
// logging macros
#define PX_LOG(...) std::cout << std::format(__VA_ARGS__) << std::endl;

#define PX_TRACE(...)                                                          \
    std::cout << GREEN << std::format(__VA_ARGS__) << RESET << std::endl;

#define PX_WARN(...)                                                           \
    std::cout << YELLOW << PX_ICON_WARN << " " << std::format(__VA_ARGS__)     \
              << RESET << std::endl;

#define PX_ERROR(...)                                                          \
    std::cerr << RED << PX_ICON_SKULL << " " << std::format(__VA_ARGS__)       \
              << RESET << std::endl;

// macros for pretty debugging
#define PX_BEGINSTEPS(...)                                                     \
    std::cout << CYAN << PX_ICON_STEPIN << " " << std::format(__VA_ARGS__)     \
              << RESET << std::endl;

#define PX_ENDSTEPS() std::cout << std::endl;

#define PX_STEPSUCCESS(...)                                                    \
    std::cout << BOLDGREEN << PX_ICON_SUCCESS << "  "                          \
              << std::format(__VA_ARGS__) << RESET << std::endl;

#define PX_STEPWARN(...)                                                       \
    std::cout << BOLDYELLOW << PX_ICON_WARN << "  "                            \
              << std::format(__VA_ARGS__) << RESET << std::endl;

#define PX_STEPFAILURE(...)                                                    \
    std::cerr << BOLDRED << PX_ICON_FAILURE << "  "                            \
              << std::format(__VA_ARGS__) << RESET << std::endl;               \
    PX_ENDSTEPS()

#else
// remove logging if not debugging
#define PX_LOG(...)
#define PX_TRACE(...)
#define PX_WARN(...)
#define PX_ERROR(...)

#define PX_BEGINSTEPS(...)
#define PX_STEPSUCCESS(...)
#define PX_STEPFAILURE(...)
#define PX_ENDSTEPS()
#endif

// Defining DEBUG_BREAK
#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define DEBUG_BREAK() __builtin_trap()
#else
#include <csignal>
#define DEBUG_BREAK() raise(SIGTRAP)
#endif

// Setting up Asserts
#ifdef PX_ENABLE_ASSERTS
#define PX_ASSERT(x, ...)                                                      \
    {                                                                          \
        if (!(x)) {                                                            \
            PX_ERROR("Assertion Failed: {0}", __VA_ARGS__);                    \
            DEBUG_BREAK();                                                     \
        }                                                                      \
    }
#else
#define PX_ASSERT(x, ...)
#endif

} // namespace Pyxis

namespace glm {
// override < operator for glm ivec2
inline bool operator<(const glm::ivec2 &lhs, const glm::ivec2 &rhs) {
    if (lhs.x < rhs.x)
        return true;
    if (lhs.x > rhs.x)
        return false;
    return lhs.y < rhs.y;
}

// override == operator for glm ivec2
inline bool operator==(const glm::ivec2 &lhs, const glm::ivec2 &rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
// Serialize glm::vec2
inline void to_json(json &j, const glm::vec2 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}};
}
// Deserialize glm::vec2
inline void from_json(const json &j, glm::vec2 &vec) {
    vec.x = j.at("x").get<float>();
    vec.y = j.at("y").get<float>();
}

// Serialize glm::ivec2
inline void to_json(json &j, const glm::ivec2 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}};
}
// Deserialize glm::ivec2
inline void from_json(const json &j, glm::ivec2 &vec) {
    vec.x = j.at("x").get<int>();
    vec.y = j.at("y").get<int>();
}

// Serialize glm::vec3
inline void to_json(json &j, const glm::vec3 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
}
// Deserialize glm::vec3
inline void from_json(const json &j, glm::vec3 &vec) {
    vec.x = j.at("x").get<float>();
    vec.y = j.at("y").get<float>();
    vec.z = j.at("z").get<float>();
}

// Serialize glm::ivec3
inline void to_json(json &j, const glm::ivec3 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
}
// Deserialize glm::ivec3
inline void from_json(const json &j, glm::ivec3 &vec) {
    vec.x = j.at("x").get<int>();
    vec.y = j.at("y").get<int>();
    vec.z = j.at("z").get<int>();
}

// Serialize glm::vec4
inline void to_json(json &j, const glm::vec4 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}, {"w", vec.w}};
}
// Deserialize glm::vec4
inline void from_json(const json &j, glm::vec4 &vec) {
    vec.x = j.at("x").get<float>();
    vec.y = j.at("y").get<float>();
    vec.z = j.at("z").get<float>();
    vec.w = j.at("w").get<float>();
}

// Serialize glm::ivec4
inline void to_json(json &j, const glm::ivec4 &vec) {
    j = json{{"x", vec.x}, {"y", vec.y}, {"z", vec.z}, {"w", vec.w}};
}
// Deserialize glm::ivec4
inline void from_json(const json &j, glm::ivec4 &vec) {
    vec.x = j.at("x").get<int>();
    vec.y = j.at("y").get<int>();
    vec.z = j.at("z").get<int>();
    vec.w = j.at("w").get<int>();
}

} // namespace glm
