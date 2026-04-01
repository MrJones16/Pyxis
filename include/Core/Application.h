#pragma once

#include <queue>
#include <string>

#include <SDL3/SDL.h>

namespace Pyxis
{

	class Application
	{
	public:
		Application(const std::string& name = "Pyxis-Engine", uint32_t width = 1280, uint32_t height = 720, const std::string& iconPath = "");
		Application(const std::string& name, bool consoleOnly);
		virtual ~Application();

		void Close();
		void Run();

		inline static Application& Get() { return *s_Instance; }

	private:
		bool m_Running = true;
		static Application* s_Instance;
	};

	//define in client/game
	Application* CreateApplication();
}

