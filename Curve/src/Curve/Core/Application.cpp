#include "cvpch.h"
#include "Application.h"

#include "Time.h"

#include "Curve/Renderer/Renderer.h"

namespace cv {

	Application::Application(const ApplicationSpecification& spec)
		: m_Specification(spec), m_Window(spec.WindowTitle, spec.WindowWidth, spec.WindowHeight)
	{
		s_Instance = this;

		m_Window.SetEventCallback([this](Event& event) { this->OnEvent(event); });

		m_Renderer = Renderer::Create(m_Window);
	}

	Application::~Application()
	{
		m_LayerStack.Clear();

		delete m_Renderer;
	}

	void Application::Run()
	{
		m_Window.Show();

		while (m_Running)
		{
			float time = Time::GetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			m_Renderer->BeginFrame();
			if (!m_Minimized)
			{
				for (Layer* layer : m_LayerStack)
					layer->OnUpdate(timestep);
			}

			// imgui here

			m_Renderer->EndFrame();

			m_Window.OnUpdate();
		}

		m_Window.Hide();
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) { return this->OnWindowCloseEvent(event); });
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& event)
		{
			if (event.GetWidth() == 0 || event.GetHeight() == 0)
			{
				m_Minimized = true;
				return false;
			}
			m_Minimized = false;
			return false;
		});

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
		{
			(*--it)->OnEvent(event);
			if (event.Handled)
				break;
		}
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	bool Application::OnWindowCloseEvent(WindowCloseEvent& event)
	{
		m_Running = false;
		return false;
	}

}
