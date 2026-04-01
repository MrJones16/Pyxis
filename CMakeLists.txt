cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#PRIOR, ON WINDOWS
if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/Peyton/Documents/GitHub/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

if(UNIX)
    set(CMAKE_TOOLCHAIN_FILE "~/git/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_COMPILER "clang++")
endif()

project("PyxisWorkspace")
    # Make compilier more strict
    #add_compile_options(
    #    $<$<CXX_COMPILER_ID:GNU,Clang>:
    #    -Wall -Wextra -Wpedantic -Wreturn-type
    #    >
    #)

    add_library(PyxisEngine)
    add_subdirectory(src)

    #find_package(GameNetworkingSockets CONFIG REQUIRED)
    find_package(box2d CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(Snappy CONFIG REQUIRED)

    # include Steamworks SDK instead of GameNetworkingSockets
    target_include_directories(PyxisEngine PUBLIC Steam)
    target_link_directories(PyxisEngine PUBLIC Steam)
    target_link_libraries(PyxisEngine PUBLIC steam_api)
    if(WIN32)
        target_link_directories(PyxisEngine PUBLIC Steam/win64)
        target_link_libraries(PyxisEngine PUBLIC steam_api64)
    endif()

    # GameNetworkingSockets static library requires both static and GameNetworkingSockets::GameNetworkingSockets targets
    target_link_libraries(PyxisEngine 
        PUBLIC 
        box2d::box2d
        #GameNetworkingSockets::static
        #GameNetworkingSockets::GameNetworkingSockets
        SDL3::SDL3
        Snappy::snappy
    )

    # Set the include directory to current directory.
    target_include_directories(PyxisEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    target_sources(PyxisEngine PRIVATE
    
    )
    

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(PyxisEngine PUBLIC PX_DEBUG)
        
        #target_link_options(Pyxis-Game PRIVATE
        #-fsanitize=address,undefined
        #)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_definitions(PyxisEngine PUBLIC PX_RELEASE)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(PyxisEngine PUBLIC PX_DIST)
    endif()

    
    #Specify the current platform!
    if(WIN32)
        target_compile_definitions(PyxisEngine PRIVATE PX_PLATFORM_WINDOWS=1)
    elseif(APPLE)
        target_compile_definitions(PyxisEngine PRIVATE PX_PLATFORM_APPLE=1)
    else(UNIX)
        target_compile_definitions(PyxisEngine PRIVATE PX_PLATFORM_LINUX=1)
    endif()
    
    target_compile_definitions(PyxisEngine PRIVATE GLFW_INCLUDE_NONE _CRT_SECURE_NO_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
