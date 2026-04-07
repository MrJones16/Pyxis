cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (UNIX)
        set(CMAKE_TOOLCHAIN_FILE "~/git/vcpkg/scripts/buildsystems/vcpkg.cmake")
    elseif (WIN32)
        set(CMAKE_TOOLCHAIN_FILE "C:/Users/Peyton/Documents/GitHub/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()




set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

project(Pyxis VERSION 0.1.0
    DESCRIPTION "Pyxis Game Engine"
    LANGUAGES CXX
)


################### Define Library ###################
add_library(Pyxis STATIC
    src/Core/Application.cpp
)

################### Include packages ###################
#find_package(GameNetworkingSockets CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(Snappy CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)


target_link_libraries(Pyxis 
    PUBLIC 
    box2d::box2d
    glm::glm
    # Note: GameNetworkingSockets requires both!
    #GameNetworkingSockets::static
    #GameNetworkingSockets::GameNetworkingSockets
    SDL3::SDL3
    Snappy::snappy
)

################### Define Build Types ###################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Pyxis PUBLIC PX_DEBUG)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(Pyxis PUBLIC PX_RELEASE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(Pyxis PUBLIC PX_DIST)
endif()

################### Define Platform ###################
if(WIN32)
    target_compile_definitions(Pyxis PRIVATE PX_PLATFORM_WINDOWS=1)
elseif(APPLE)
    target_compile_definitions(Pyxis PRIVATE PX_PLATFORM_APPLE=1)
else(UNIX)
    target_compile_definitions(Pyxis PRIVATE PX_PLATFORM_LINUX=1)
endif()


target_include_directories(Pyxis PUBLIC
    # During build: use the source layout
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # After install: use the installed layout
    $<INSTALL_INTERFACE:include>
)


target_compile_definitions(Pyxis PRIVATE _CRT_SECURE_NO_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

# ----------------------------------------------------------------------
# Installation + package config (this is what makes find_package work)
# ----------------------------------------------------------------------
install(TARGETS Pyxis
    EXPORT PyxisTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    #possibly add:
    #INCLUDES DESTINATION include
)

# Install the headers
install(DIRECTORY include/
    DESTINATION include
)

# Export the targets
install(EXPORT PyxisTargets
    FILE PyxisTargets.cmake
    NAMESPACE Pyxis::
    DESTINATION lib/cmake/Pyxis
)

# Generate the config file that find_package will look for
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/PyxisConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PyxisConfig.cmake
    INSTALL_DESTINATION lib/cmake/Pyxis
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/PyxisConfig.cmake
    DESTINATION lib/cmake/Pyxis
)
