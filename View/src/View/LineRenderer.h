#pragma once

#include "GraphCamera.h"

#include <Curve/Renderer/Renderer.h>

#include <glm/glm.hpp>

#include <vector>
#include <functional>

namespace cv {

	struct LineVertex
	{
		glm::vec4 Position;
		glm::vec4 Color;
		int LineIndex;
		int Padding[3];
	};

	struct RendererData
	{
		Shader* LineShader = nullptr;
		Shader* LineComputeShader = nullptr;
		GraphicsPipeline* LinePipeline = nullptr;
		ComputePipeline* LineComputePipeline = nullptr;

		Buffer<VertexBuffer | StorageBuffer>* LineVertexBuffer = nullptr;
		Buffer<StorageBuffer>* LineDataBuffer = nullptr;

		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;

		std::vector<size_t> LineVertexCounts;

		std::vector<CommandBuffer> CommandBuffers = {};

		Buffer<StagingBuffer>* LineIDBuffer = nullptr;
	};

	class LineRenderer
	{
	public:
		LineRenderer() = default;
		LineRenderer(Renderer* renderer);
		LineRenderer(Renderer* renderer, Framebuffer* framebuffer);
		~LineRenderer();

		Buffer<StagingBuffer>* Render(const GraphCamera& camera);
		Buffer<StagingBuffer>* Render(const GraphCamera& camera, Framebuffer* framebuffer, const glm::vec2& relativeMousePosition);

		void AddLine(std::function<float(float)>&& f, const glm::vec4& color);

		void MoveCamera();

		bool OnWindowResize(WindowResizeEvent& event);

		const glm::vec4& GetLineColor(int index) const { return m_Lines[index > m_Lines.size() - 1 ? 0 : index].Color; }
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
