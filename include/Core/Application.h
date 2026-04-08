#pragma once

#include "Core/Timestep.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>

namespace Pyxis {

class Application {
  public:
    Application(const std::string &title = "Pyxis-Engine",
                const glm::ivec2 &resolution = glm::ivec2(1920, 1080),
                const std::string &iconPath = "");
    // Will be called by SDL app init
    bool Init();
    virtual ~Application();

    // sets running to false so app will shutdown
    void Close();

    // Override on your application implementation!
    virtual void OnUpdate(Timestep ts);

    // grabs the static instance of Application
    inline static Application &Get() { return *s_Instance; }

    inline std::string GetTitle() const { return m_Title; }
    void SetTitle(const std::string &title);

    inline glm::ivec2 GetResolution() const { return m_Resolution; }
    void SetResolution(const glm::ivec2 &resolution);

    inline bool IsRunning() const { return m_Running; }

  protected:
    std::string m_Title;
    glm::ivec2 m_Resolution;

  private:
    bool m_Running = true;
    static Application *s_Instance;
};

// define in client/game
Application *CreateApplication();
} // namespace Pyxis
