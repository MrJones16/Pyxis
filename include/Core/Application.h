#pragma once

#include "Core/Timestep.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>

namespace Pyxis {

/// Application is the root class that a game or app defines.
///
/// you must define Application *CreateApplication(); in your cpp file
///
/// You then can override the default methods like OnUpdate, OnTick, OnEvent,
/// etc.
///
/// You can set a tickrate for the OnTick function with SetTickRate.
///
class Application {
  public:
    Application(const std::string &title = "Pyxis-Engine",
                const glm::ivec2 resolution = glm::ivec2(1920, 1080),
                const std::string &iconPath = "");

    // Will be called by SDL app init
    bool Init();
    virtual ~Application();
    // sets running to false so app will shutdown
    void Close();
    // RunFrame will call the OnUpdate, and then manage the tick rate updates
    void RunFrame(Timestep ts);

    // Override on your application implementation!
    virtual void OnInit() {};
    // make sure to shutdown renderers and remove all textures!
    virtual void OnShutdown() {};
    virtual void OnUpdate(Timestep ts) {};
    virtual void OnTick() {};
    virtual void OnEvent(SDL_Event *event) {};
    virtual void OnWindowResize(const glm::ivec2 &resolution) {};

    // grabs the static instance of Application
    inline static Application &Get() { return *s_Instance; }

    inline static std::string GetTitle() { return s_Instance->m_Title; }
    static void SetTitle(const std::string &title);

    static void SetWindowSize(glm::ivec2 size);

    static void StartTextInput();
    static void StopTextInput();

    inline bool IsRunning() const { return m_Running; }

    inline void SetTickRate(double ticksPerSecond) {
        m_TicksPerSecond = ticksPerSecond;
    }

  protected:
    std::string m_Title;
    glm::ivec2 m_Resolution;
    double m_TicksPerSecond = 60;
    double m_AccumulatedTickTime = 0;

  private:
    static Application *s_Instance;
    bool m_Running = true;
};

// define in client/game
Application *CreateApplication();
} // namespace Pyxis
