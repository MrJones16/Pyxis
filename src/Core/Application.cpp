#include <Core/Application.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <box2d/id.h>
#include <iostream>
#include <memory>

#include <box2d/box2d.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

namespace Pyxis {

extern Application *CreateApplication();

Application *Application::s_Instance = nullptr;

Application::Application(const std::string &title, const glm::ivec2 &resolution,
                         const std::string &iconPath)
    : m_Title(title), m_Resolution(resolution) {
    assert(!s_Instance);
    b2WorldDef def = b2DefaultWorldDef();
    b2WorldId id = b2CreateWorld(&def);
    // PX_CORE_ASSERT(!s_Instance, "Application already exists!");
}

// Application::Application(const std::string &title, bool consoleOnly) {}

Application::~Application() {}

void Application::Close() { m_Running = false; }

void Application::OnUpdate(Timestep ts) {}

void Application::SetWindow(SDL_Window *window) { m_Window = window; }

void Application::SetTitle(const std::string &title) {
    m_Title = title;
    SDL_SetWindowTitle(m_Window, title.c_str());
}

void Application::SetResolution(const glm::ivec2 &resolution) {
    m_Resolution = resolution;
    SDL_SetWindowSize(m_Window, m_Resolution.x, m_Resolution.y);
}

// bool Application::OnWindowClose(WindowCloseEvent& e)
// {
// 	Close();
// 	return true;
// }

} // namespace Pyxis
  //
SDL_Renderer *renderer;
SDL_Window *window;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    Pyxis::Application *app = Pyxis::CreateApplication();
    *appstate = app;

    std::string title = app->GetTitle();
    glm::ivec2 resolution = app->GetResolution();
    SDL_SetAppMetadata(title.c_str(), "0.1", "");

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(title.c_str(), resolution.x, resolution.y,
                                     SDL_WINDOW_RESIZABLE, &window,
                                     &renderer)) {

        SDL_Log("Unable to initialize SDL Window & Renderer: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app->SetWindow(window);
    SDL_Log("Created window and renderer!");

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

    SDL_SetRenderDrawColorFloat(renderer, 0.5, 0.5, 0.5,
                                SDL_ALPHA_OPAQUE_FLOAT);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    // close the window on request
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {

    Pyxis::Application *app = static_cast<Pyxis::Application *>(appstate);
    app->Close();
    // destroy the window
    SDL_DestroyWindow(app->GetWindow());
}
