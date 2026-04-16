#include <Core/Application.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <Core/Input.h>
#include <Renderer/Renderer.h>

namespace Pyxis {

extern Application *CreateApplication();

Application *Application::s_Instance = nullptr;

Application::Application(const std::string &title, const glm::ivec2 &resolution,
                         const std::string &iconPath)
    : m_Title(title), m_Resolution(resolution) {
    assert(!s_Instance);

    PX_ASSERT(!s_Instance, "Application already exists!");
}

bool Application::Init() {
    SDL_SetAppMetadata(m_Title.c_str(), "0.1", "");

    // SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
    int num = SDL_GetNumGPUDrivers();
    PX_TRACE("Number of drivers: {}", num);
    for (int i = 0; i < num; i++) {
        std::string name = (std::string)SDL_GetGPUDriver(i);
        PX_TRACE("GPU DRIVER {}: {}", i, name);
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        PX_ERROR("Unable to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!Renderer::Init(m_Title, m_Resolution)) {
        PX_ERROR("Unable to initialize renderer!");
        return false;
    }

    PX_TRACE("Created window and Initialized Renderer!");
    return true;
}

Application::~Application() {

    // shutdown renderer
    // this destroys window and gpu device
    Renderer::Shutdown();
}

void Application::Close() { m_Running = false; }

void Application::OnUpdate(Timestep ts) {}

void Application::OnEvent(SDL_Event *event) {}

} // namespace Pyxis

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    Pyxis::Application *app = Pyxis::CreateApplication();
    *appstate = app;
    if (!app->Init()) {
        PX_ERROR("Application failed to initialize!");
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_Time previousTime;
SDL_AppResult SDL_AppIterate(void *appstate) {
    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    SDL_Time time;
    SDL_GetCurrentTime(&time);
    Pyxis::Timestep ts = SDL_NS_TO_SECONDS(time - previousTime);
    previousTime = time;
    app->OnUpdate(ts);
    if (!app->IsRunning())
        return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    // close the window on request
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        return SDL_APP_SUCCESS;
    }

    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    app->OnEvent(event);

    // TODO: implement more event handling

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {

    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    app->Close();

    delete app;
}
