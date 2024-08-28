#pragma once

#include "Base.h"
#include "Window.h"
#include "LayerStack.h"
#include "Timestep.h"

#include "Curve/ImGui/ImGuiLayer.h"

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

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		void Exit() { m_Running = false; }

		Renderer* GetRenderer() { return m_Renderer; }
		
		static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowCloseEvent(WindowCloseEvent& event);
	private:
		ApplicationSpecification m_Specification;
		Window m_Window;

		Renderer* m_Renderer = nullptr;

		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

		ImGuiLayer* m_ImGuiLayer = nullptr;

		bool m_Minimized = false;
		bool m_Running = true;
	private:
		inline static Application* s_Instance = nullptr;
	};

	Application* CreateApplication(int argc, char** argv);

}
