#include "ViewLayer.h"

#include <Curve/Core/Application.h>
#include <Curve/Renderer/Renderer.h>

#include <imgui_internal.h>

namespace cv {

	ViewLayer::ViewLayer()
		: Layer("View Layer")
	{
	}

	ViewLayer::~ViewLayer()
	{
	}

	static glm::vec4 Lerp(const glm::vec4& start, const glm::vec4& end, float t)
	{
		return start + t * (end - start);
	}

	void ViewLayer::OnAttach()
	{
		Renderer* renderer = Application::Get().GetRenderer();

#ifdef CV_PLATFORM_WINDOWS
		renderer->GetWindow().SetWindowAttribute(WindowAttribute::TitlebarColor, { 0.0f, 0.0f, 0.0f, 1.0f });
		m_OriginalBorderColor = m_CurrentBorderColor = m_TargetBorderColor = renderer->GetWindow().GetWindowAttribute(WindowAttribute::BorderColor);
#endif

		FramebufferSpecification spec{};
		spec.Attachments = { AttachmentFormat::Default, AttachmentFormat::R32SInt, AttachmentFormat::Depth };
		spec.Width = 800;
		spec.Height = 600;
		spec.Multisample = true;

		m_Framebuffer = renderer->CreateFramebuffer(spec);

		m_LineRenderer = new LineRenderer(renderer, m_Framebuffer);
		m_LineRenderer->AddLine([](float x) { return x * cos(x) * sin(x); }, { 1.0f, 1.0f, 1.0f, 1.0f });
		m_LineRenderer->AddLine([](float x) { return x * sin(x); }, { 1.0f, 1.0f, 1.0f, 1.0f });

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
		void* previousWindow = Input::GetActiveWindow();
		if (m_ViewportWindowHandle)
			Input::SetActiveWindow(m_ViewportWindowHandle);

		if (m_IDBuffer)
		{
			int* idData = (int*)m_IDBuffer->Map(m_IDBuffer->GetSize());
			int id = *idData;
			m_IDBuffer->Unmap();

			m_ID = id;
			if (m_ID != 0)
				m_TargetBorderColor = m_LineRenderer->GetLineColor(m_ID - 1);
			else
				m_TargetBorderColor = m_OriginalBorderColor;
		}

		if ((uint32_t)m_ViewportSize.x != m_Framebuffer->GetWidth() || (uint32_t)m_ViewportSize.y != m_Framebuffer->GetHeight())
		{
			if (!Input::IsMouseButtonDown(MouseButton::ButtonLeft))
			{
				m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				WindowResizeEvent e{ (uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y };
				m_LineRenderer->OnWindowResize(e);
			}
			m_Camera.OnResize(m_ViewportSize.x, m_ViewportSize.y);
		}

		m_IDBuffer = m_LineRenderer->Render(m_Camera, m_Framebuffer, m_RelativeMousePos);

		if (m_Camera.OnUpdate(ts, m_ViewportTitlebarHovered || m_CameraMovedLastFrame))
		{
			m_LineRenderer->MoveCamera();
			m_CameraMovedLastFrame = true;
		}
		else
			m_CameraMovedLastFrame = false;

		Input::SetActiveWindow(previousWindow);

		if (m_CurrentBorderColor != m_TargetBorderColor)
		{
			m_CurrentBorderColor = Lerp(m_CurrentBorderColor, m_TargetBorderColor, 0.05f);
			if (m_TargetBorderColor == m_OriginalBorderColor)
				m_CurrentTitlebarColor = Lerp(m_CurrentTitlebarColor, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.05f);
			else
				m_CurrentTitlebarColor = Lerp(m_CurrentTitlebarColor, m_TargetBorderColor, 0.05f);

#ifdef CV_PLATFORM_WINDOWS
			Application::Get().GetRenderer()->GetWindow().SetWindowAttribute(WindowAttribute::BorderColor, m_CurrentBorderColor);
			Application::Get().GetRenderer()->GetWindow().SetWindowAttribute(WindowAttribute::TitlebarColor, m_CurrentTitlebarColor);
#endif
		}
	}

	static void Dockspace()
	{
		static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Dockspace", nullptr, windowFlags);
		ImGui::PopStyleVar();

		ImGui::PopStyleVar(2);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID id = ImGui::GetID("Dockspace");
			ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					Application::Get().Exit();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::End();
	}

	void ViewLayer::OnImGuiRender()
	{
		Dockspace();

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
		ImGui::Begin("Viewport", nullptr, windowFlags);
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 windowPos = ImGui::GetWindowPos();
		m_ViewportSize = { windowSize.x, windowSize.y };
		m_ViewportWindowPos = { windowPos.x, windowPos.y };

		ImVec2 mousePos = ImGui::GetMousePos();
		ImVec2 relativeMousePos = ImVec2(mousePos.x - windowPos.x, mousePos.y - windowPos.y);
		m_RelativeMousePos = { relativeMousePos.x, relativeMousePos.y };

		ImGuiStyle& style = ImGui::GetStyle();
		float titlebarHeight = ImGui::GetFontSize() + style.FramePadding.y * 2;

		ImRect titlebarRect(
			windowPos,
			ImVec2(windowPos.x + windowSize.x, windowPos.y + titlebarHeight)
		);

		m_ViewportTitlebarHovered = ImGui::IsMouseHoveringRect(titlebarRect.Min, titlebarRect.Max, false);

		ImGuiViewport* windowViewport = ImGui::GetWindowViewport();
		m_ViewportWindowHandle = windowViewport->PlatformHandle;

		ImGui::InvisibleButton("viewport_framebuffer",  {(float)m_Framebuffer->GetWidth(), (float)m_Framebuffer->GetHeight() });
		ImGui::SetCursorPos({ 0, 0 });
		ImGui::Image(m_Framebuffer->GetCurrentDescriptor(), { (float)m_Framebuffer->GetWidth(), (float)m_Framebuffer->GetHeight() });
		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::Begin("id");
		ImGui::Text(std::to_string(m_ID).c_str());
		ImGui::End();
	}

	void ViewLayer::OnEvent(Event& e)
	{
		if (m_Camera.OnEvent(e, m_ViewportTitlebarHovered))
			m_LineRenderer->MoveCamera();
	}

}
