#pragma once

#include "Core/Timestep.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>

namespace Pyxis {

class Application {
  public:
    Application(const std::string &title = "Pyxis-Engine",
                const glm::ivec2 resolution = glm::ivec2(1920, 1080),
                const std::string &iconPath = "");

    // Will be called by SDL app init
    bool Init();
    // overridable init for game, called afer actual init calls
    virtual void OnInit();
    // overridable shutdown to be called before actual shutdown.
    // make sure to shutdown renderers and remove all textures!
    virtual void OnShutdown();

    virtual ~Application();

    // sets running to false so app will shutdown
    void Close();

    // Override on your application implementation!
    // Called once per rendered frame (input, camera, rendering).
    virtual void OnUpdate(Timestep ts);

    ////////////////////
    /// FIXED TICKS ///
    ////////////////////
    // Called at a fixed rate (default 60 ticks per second), before OnUpdate in
    // the same frame. Use it for deterministic simulation (physics, falling
    // sand, netcode). `tick` is the index of this tick (0, 1, 2, ...).
    virtual void OnTick(uint64_t tick);

    // Ticks per second (default 60). Resets the accumulator.
    void SetTickRate(int ticksPerSecond);
    inline int GetTickRate() const { return m_TickRate; }
    // Fixed time per tick in seconds (1 / tick rate).
    inline float GetTickDelta() const { return 1.0f / (float)m_TickRate; }
    // Number of ticks run so far (the next tick's index).
    inline uint64_t GetTickCount() const { return m_TickCount; }
    // Max ticks run in a single frame when catching up; leftover time is
    // dropped so a slow frame can't snowball (default 5).
    inline void SetMaxTicksPerFrame(int maxTicks) {
        m_MaxTicksPerFrame = maxTicks < 1 ? 1 : maxTicks;
    }
    inline int GetMaxTicksPerFrame() const { return m_MaxTicksPerFrame; }
    // How far (0..1) the current frame is between the last tick and the next,
    // useful for interpolating rendering.
    inline float GetTickAlpha() const {
        return (float)(m_TickAccumulator * m_TickRate);
    }

    // Runs the due ticks for this frame, then OnUpdate. Called by the engine
    // main loop every frame.
    void RunFrame(Timestep ts);
    virtual void OnEvent(SDL_Event *event);
    virtual void OnWindowResize(const glm::ivec2 &resolution);

    // grabs the static instance of Application
    inline static Application &Get() { return *s_Instance; }

    inline static std::string GetTitle() { return s_Instance->m_Title; }
    static void SetTitle(const std::string &title);

    static void SetWindowSize(glm::ivec2 size);

    static void StartTextInput();
    static void StopTextInput();

    inline bool IsRunning() const { return m_Running; }

  protected:
    std::string m_Title;
    glm::ivec2 m_Resolution;

  private:
    static Application *s_Instance;
    bool m_Running = true;

    int m_TickRate = 60;
    int m_MaxTicksPerFrame = 5;
    uint64_t m_TickCount = 0;
    double m_TickAccumulator = 0.0; // seconds of un-ticked time
};

// define in client/game
Application *CreateApplication();
} // namespace Pyxis
