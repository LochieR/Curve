#pragma once

#include "Base.h"
#include "Window.h"

#include <string>
#include <vector>

namespace cv {

	struct ApplicationCommandLineArgs
	{
		std::vector<const char*> Args;

		const char* operator[](size_t index) const { return Args[index]; }

		std::vector<const char*>::iterator begin() { return Args.begin(); }
		std::vector<const char*>::iterator end() { return Args.end(); }
		std::vector<const char*>::const_iterator begin() const { return Args.begin(); }
		std::vector<const char*>::const_iterator end() const { return Args.end(); }

		ApplicationCommandLineArgs() = default;

		ApplicationCommandLineArgs(int argc, char** argv)
			: Args(argv, argv + argc)
		{
		}
	};

	struct ApplicationSpecification
	{
		ApplicationCommandLineArgs CommandLineArgs;

		uint32_t WindowWidth = 1280, WindowHeight = 720;
		std::string WindowTitle = "Curve Window";

		bool UseImGui = true;
		bool UseDefaultTitlebar = true;

		struct
		{
		} WindowsPlatformSettings;
	};

	class Renderer;

	class Application
	{
	public:
		Application(const ApplicationSpecification& spec);
		~Application();

		void Run();

		void OnEvent(Event& event);
	private:
		bool OnWindowCloseEvent(WindowCloseEvent& event);
	private:
		ApplicationSpecification m_Specification;
		Window m_Window;

		Renderer* m_Renderer = nullptr;

		bool m_Running = true;
	};

	Application* CreateApplication(int argc, char** argv);

}
