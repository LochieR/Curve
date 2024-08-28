#pragma once

#include "GraphCamera.h"
#include "LineRenderer.h"

#include <Curve/Core/Layer.h>
#include <Curve/Renderer/Framebuffer.h>

#include <glm/glm.hpp>

namespace cv {

	class ViewLayer : public Layer
	{
	public:
		ViewLayer();
		virtual ~ViewLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;
	private:
		LineRenderer* m_LineRenderer = nullptr;
		GraphCamera m_Camera;

		Framebuffer* m_Framebuffer = nullptr;
		Buffer<StagingBuffer>* m_IDBuffer = nullptr;

		void* m_ViewportWindowHandle = nullptr;
		bool m_ViewportTitlebarHovered = false;
		bool m_CameraMovedLastFrame = false;

		glm::vec2 m_ViewportSize = { 800, 600 };
		glm::vec2 m_ViewportWindowPos = { 0, 0 };
		glm::vec2 m_RelativeMousePos = { 0, 0 };
		
		glm::vec4 m_OriginalBorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec4 m_CurrentBorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec4 m_CurrentTitlebarColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec4 m_TargetBorderColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		int m_ID = 0;
	};

}
