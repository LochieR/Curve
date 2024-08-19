#include "ViewLayer.h"

#include <Curve/Core/Application.h>
#include <Curve/Renderer/Renderer.h>

namespace cv {

	ViewLayer::ViewLayer()
		: Layer("View Layer")
	{
	}

	ViewLayer::~ViewLayer()
	{
	}

	void ViewLayer::OnAttach()
	{
		Renderer* renderer = Application::Get().GetRenderer();

#ifdef CV_PLATFORM_WINDOWS
		renderer->GetWindow().SetWindowAttribute(WindowAttribute::TitlebarColor, { 0.0f, 0.0f, 0.0f, 1.0f });
#endif

		m_LineRenderer = new LineRenderer(renderer);
		m_LineRenderer->AddLine([](float x) { return x * cos(x) * sin(x); }, { 0.3f, 0.2f, 0.8f, 1.0f });
		m_LineRenderer->AddLine([](float x) { return x * sin(x); }, { 0.8f, 0.2f, 0.3f, 1.0f });

		Window& window = renderer->GetWindow();
		m_Camera = GraphCamera((float)window.GetWidth(), (float)window.GetHeight());
	}

	void ViewLayer::OnDetach()
	{
		delete m_LineRenderer;
	}

	void ViewLayer::OnUpdate(Timestep ts)
	{
		m_LineRenderer->Render(m_Camera);

		if (m_Camera.OnUpdate(ts))
			m_LineRenderer->MoveCamera();
	}

	void ViewLayer::OnImGuiRender()
	{
	}

	void ViewLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return m_LineRenderer->OnWindowResize(e); });

		if (m_Camera.OnEvent(e))
			m_LineRenderer->MoveCamera();
	}

}
