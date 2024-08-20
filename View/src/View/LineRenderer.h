#pragma once

#include "GraphCamera.h"

#include <Curve/Renderer/Renderer.h>

#include <glm/glm.hpp>

#include <vector>
#include <functional>

namespace cv {

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	struct RendererData
	{
		Shader* LineShader = nullptr;
		GraphicsPipeline* LinePipeline = nullptr;

		VertexBuffer* LineVertexBuffer = nullptr;

		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;

		std::vector<size_t> LineVertexCounts;

		std::vector<CommandBuffer> CommandBuffers = {};
	};

	class LineRenderer
	{
	public:
		LineRenderer() = default;
		LineRenderer(Renderer* renderer);
		LineRenderer(Renderer* renderer, Framebuffer* framebuffer);
		~LineRenderer();

		void Render(const GraphCamera& camera);
		void Render(const GraphCamera& camera, Framebuffer* framebuffer);

		void AddLine(std::function<float(float)>&& f, const glm::vec4& color);

		void MoveCamera();

		bool OnWindowResize(WindowResizeEvent& event);
	private:
		Renderer* m_Renderer = nullptr;
		RendererData m_Data;

		std::vector<bool> m_RecordCommandBuffer;
		bool m_Redraw = true;

		struct Line
		{
			std::function<float(float)> Function;
			glm::vec4 Color;
		};

		std::vector<Line> m_Lines;
	};

}
