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

		glm::vec2 m_ViewportSize = { 800, 600 };
	};

}
