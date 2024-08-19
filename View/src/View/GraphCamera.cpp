#include "GraphCamera.h"

namespace cv {

	GraphCamera::GraphCamera(float width, float height)
		: m_ViewMatrix(1.0f)
	{
		OnResize(width, height);
	}

	GraphCamera::GraphCamera(float left, float right, float bottom, float top)
		: m_ViewMatrix(1.0f)
	{
		SetProjection(left, right, bottom, top);
		RecalculateViewMatrix();
	}

	GraphCamera::GraphCamera(float left, float right, float bottom, float top, float near, float far)
		: m_ViewMatrix(1.0f)
	{
		SetProjection(left, right, bottom, top, near, far);
		RecalculateViewMatrix();
	}

	bool GraphCamera::OnUpdate(Timestep ts)
	{
		glm::vec2 mousePosition = Input::GetMousePosition();
		glm::vec2 delta = m_MousePosition - mousePosition;
		m_MousePosition = mousePosition;

		if (Input::IsMouseButtonDown(MouseButton::ButtonLeft))
		{
			delta *= m_ZoomLevel;
			SetPosition(m_Position - glm::vec3(-delta.x * 0.003f, delta.y * 0.003f, 0.0f));
			return true;
		}
		return false;
	}

	bool GraphCamera::OnEvent(Event& event)
	{
		bool changed = false;

		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<MouseScrolledEvent>([this, &changed](MouseScrolledEvent& event)
		{
			m_ZoomLevel -= event.GetYOffset() * 0.25f;
			m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);
			SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);

			changed = true;
			return false;
		});
		dispatcher.Dispatch<WindowResizeEvent>([this, &changed](WindowResizeEvent& event)
		{
			OnResize((float)event.GetWidth(), (float)event.GetHeight());

			changed = true;
			return false;
		});

		return changed;
	}

	void GraphCamera::OnResize(float width, float height)
	{
		m_AspectRatio = width / height;
		SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
	}

	void GraphCamera::SetProjection(float left, float right, float bottom, float top)
	{
		m_ProjectionMatrix = glm::ortho(left, right, top, bottom, -1.0f, 1.0f);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

	void GraphCamera::SetProjection(float left, float right, float bottom, float top, float near, float far)
	{
		m_ProjectionMatrix = glm::ortho(left, right, top, bottom, near, far);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

	void GraphCamera::RecalculateViewMatrix()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position)
			* glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

}
