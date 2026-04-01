#include "Application.h"
#include <iostream>

namespace Pyxis 
{

	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string& name, uint32_t width, uint32_t height, const std::string& iconPath) {

		//PX_CORE_ASSERT(!s_Instance, "Application already exists!");
		
	}

	Application::Application(const std::string& name, bool consoleOnly) {

		
	}

	Application::~Application() {
		//Pyxis::Network::Network_Shutdown();
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::Run() {
		while (m_Running)
		{
			// float time = (float)glfwGetTime();
			// Timestep timestep = time - m_LastFrameTime;
			// m_LastFrameTime = time;
			std::cout << "Running!" << std::endl;
		}
	}

	// bool Application::OnWindowClose(WindowCloseEvent& e)
	// {
	// 	Close();
	// 	return true;
	// }

}