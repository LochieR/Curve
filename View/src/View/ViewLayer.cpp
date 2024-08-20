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

		FramebufferSpecification spec{};
		spec.Attachments = { AttachmentFormat::Default, AttachmentFormat::Depth };
		spec.Width = 800;
		spec.Height = 600;
		spec.Multisample = true;

		m_Framebuffer = renderer->CreateFramebuffer(spec);

		m_LineRenderer = new LineRenderer(renderer, m_Framebuffer);
		m_LineRenderer->AddLine([](float x) { return x * cos(x) * sin(x); }, { 0.3f, 0.2f, 0.8f, 1.0f });
		m_LineRenderer->AddLine([](float x) { return x * sin(x); }, { 0.8f, 0.2f, 0.3f, 1.0f });

		Window& window = renderer->GetWindow();
		m_Camera = GraphCamera((float)window.GetWidth(), (float)window.GetHeight());
	}

	void ViewLayer::OnDetach()
	{
		delete m_LineRenderer;
		delete m_Framebuffer;
	}

	void ViewLayer::OnUpdate(Timestep ts)
	{
		if ((uint32_t)m_ViewportSize.x != m_Framebuffer->GetWidth() || (uint32_t)m_ViewportSize.y != m_Framebuffer->GetHeight())
		{
			if (!Input::IsMouseButtonDown(MouseButton::ButtonLeft))
				m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_Camera.OnResize(m_ViewportSize.x, m_ViewportSize.y);
			m_LineRenderer->MoveCamera();
		}

		m_LineRenderer->Render(m_Camera, m_Framebuffer);

		if (m_Camera.OnUpdate(ts))
			m_LineRenderer->MoveCamera();
	}

	void ViewLayer::OnImGuiRender()
	{
		ImGui::Begin("Hello");
		ImGui::Text("Text");
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImVec2 size = ImGui::GetWindowSize();
		m_ViewportSize = { size.x, size.y };

		ImGui::Image(m_Framebuffer->GetCurrentDescriptor(), { (float)m_Framebuffer->GetWidth(), (float)m_Framebuffer->GetHeight() });
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewLayer::OnEvent(Event& e)
	{
		if (m_Camera.OnEvent(e))
			m_LineRenderer->MoveCamera();
	}

}
