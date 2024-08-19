#pragma once

#include <Curve/Core/Event.h>
#include <Curve/Core/Timestep.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace cv {

	class GraphCamera
	{
	public:
		GraphCamera() = default;
		GraphCamera(float width, float height);
		GraphCamera(float left, float right, float bottom, float top);
		GraphCamera(float left, float right, float bottom, float top, float near, float far);

		bool OnUpdate(Timestep ts);
		bool OnEvent(Event& event);

		void OnResize(float width, float height);

		void SetProjection(float left, float right, float bottom, float top);
		void SetProjection(float left, float right, float bottom, float top, float near, float far);

		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }

		float GetRotation() const { return m_Rotation; }
		void SetRotation(float degrees) { m_Rotation = degrees; RecalculateViewMatrix(); }

		float GetZoomLevel() const { return m_ZoomLevel; }

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
	private:
		void RecalculateViewMatrix();
	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec2 m_MousePosition = { 0.0f, 0.0f };

		float m_AspectRatio;
		float m_ZoomLevel = 1.0f;

		glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
		float m_Rotation = 0.0f;
	};

}
