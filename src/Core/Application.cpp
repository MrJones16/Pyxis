#include <cmath>
#include <SDL3/SDL_events.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Core/Entity.h"
#include <Core/Application.h>
#include <Core/Input.h>
#include <Core/UI.h>
#include <Renderer/DefaultRenderer.h>

namespace Pyxis {

extern Application *CreateApplication();

Application *Application::s_Instance = nullptr;

Application::Application(const std::string &title, const glm::ivec2 resolution,
                         const std::string &iconPath)
    : m_Title(title) {
    m_Resolution = resolution;
    PX_ASSERT(!s_Instance, "Application already exists!");
    s_Instance = this;
}

bool Application::Init() {
    PX_BEGINSTEPS("Initializing Application...");
    SDL_SetAppMetadata(m_Title.c_str(), "0.1", "");

    // SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
    int num = SDL_GetNumGPUDrivers();
    PX_LOG("# GPU Drivers: {}", num);
    for (int i = 0; i < num; i++) {
        std::string name = (std::string)SDL_GetGPUDriver(i);
        PX_LOG(" - GPU Driver {}: {}", i, name);
    }

#if defined(PX_PLATFORM_LINUX)
    // Prefer X11 on Linux (SDL_VIDEO_DRIVER env var still overrides this).
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        PX_ERROR("Unable to initialize SDL video: {}", SDL_GetError());
        return false;
    }
    // Audio is optional: keep running without it if no audio device/server.
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        PX_WARN("Unable to initialize SDL audio, continuing without audio: {}",
                SDL_GetError());
    }

    if (!Renderer::Init(m_Title, m_Resolution, true)) {
        PX_ERROR("Unable to initialize renderer!");
        return false;
    }
    DefaultRenderer::Init(10000);

    UI::Init();

    return true;
}

void Application::OnInit() {}
void Application::OnShutdown() {}

Application::~Application() {
    OnShutdown();

    // clear all entities
    Entity::ClearRegistry();

    DefaultRenderer::Shutdown();

    // shutdown renderer
    // this destroys window and gpu device
    Renderer::Shutdown();

    s_Instance = nullptr;
}

void Application::Close() { m_Running = false; }

void Application::OnUpdate(Timestep ts) {}
void Application::OnTick(uint64_t tick) {}

void Application::SetTickRate(int ticksPerSecond) {
    m_TickRate = ticksPerSecond < 1 ? 1 : ticksPerSecond;
    m_TickAccumulator = 0.0;
}

void Application::RunFrame(Timestep ts) {
    const double tickDelta = 1.0 / (double)m_TickRate;
    m_TickAccumulator += (double)ts.GetSeconds();

    int ticksThisFrame = 0;
    while (m_TickAccumulator >= tickDelta && m_Running) {
        if (ticksThisFrame >= m_MaxTicksPerFrame) {
            // too far behind: drop the backlog instead of spiraling
            m_TickAccumulator = std::fmod(m_TickAccumulator, tickDelta);
            break;
        }
        OnTick(m_TickCount);
        m_TickCount++;
        m_TickAccumulator -= tickDelta;
        ticksThisFrame++;
    }

    OnUpdate(ts);
}
void Application::OnEvent(SDL_Event *event) {}
// called already, don't need to try to call from OnEvent()
void Application::OnWindowResize(const glm::ivec2 &resolution) {}

void Application::StartTextInput() {
    SDL_StartTextInput(Renderer::GetWindow());
}
void Application::StopTextInput() { SDL_StopTextInput(Renderer::GetWindow()); }

void Application::SetTitle(const std::string &title) {
    s_Instance->m_Title = title;
    Renderer::SetTitle(title);
}

void Application::SetWindowSize(glm::ivec2 size) {
    s_Instance->OnWindowResize(size);
}

} // namespace Pyxis

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    Pyxis::Application *app = Pyxis::CreateApplication();
    *appstate = app;
    if (!app->Init()) {
        PX_ERROR("Application failed to initialize!");
        return SDL_APP_FAILURE;
    }
    app->OnInit();

    return SDL_APP_CONTINUE;
}

static Uint64 s_PreviousTicksNS = 0;
SDL_AppResult SDL_AppIterate(void *appstate) {
    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    if (!app->IsRunning())
        return SDL_APP_SUCCESS;

    // Frame delta in seconds as a float. (SDL_NS_TO_SECONDS is integer
    // division, which made every frame 0s; wall-clock time could also jump.)
    Uint64 nowNS = SDL_GetTicksNS();
    float deltaSeconds =
        s_PreviousTicksNS == 0
            ? 0.0f
            : (float)((double)(nowNS - s_PreviousTicksNS) / 1e9);
    s_PreviousTicksNS = nowNS;
    Pyxis::Timestep ts = deltaSeconds;
    auto vec = Pyxis::Input::GetMousePositon();
    Clay_Vector2 position = {vec.x, vec.y};
    app->RunFrame(ts);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    // close the window on request
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        return SDL_APP_SUCCESS;
    }
    Pyxis::Input::OnEvent(event);
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        glm::ivec2 resolution;
        resolution.x = event->window.data1;
        resolution.y = event->window.data2;
        // Pyxis::Renderer::OnWindowResize(resolution);
        Pyxis::UI::OnWindowResize(resolution);
        app->OnWindowResize(resolution);
    }
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        Pyxis::UI::OnMouseWheelEvent(glm::vec2(event->wheel.x, event->wheel.y));
    }

    app->OnEvent(event);

    // TODO: implement more event handling

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {

    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    app->Close();

    app->OnShutdown();
    delete app;
}
