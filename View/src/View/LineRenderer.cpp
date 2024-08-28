#include "LineRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace cv {

	static constexpr size_t s_MaxVertices = 100'000;

	LineRenderer::LineRenderer(Renderer* renderer)
		: m_Renderer(renderer)
	{
		m_Data = {};
		
		InputLayout layout{};
		layout.VertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Int,    "a_LineIndex" }
		};

		PushConstantInfo cameraPushConstant{};
		cameraPushConstant.Size = sizeof(glm::mat4);
		cameraPushConstant.Offset = 0;
		cameraPushConstant.Stage = ShaderStage::Vertex;

		layout.PushConstants.push_back(cameraPushConstant);

		m_Data.LineShader = renderer->CreateShader("Shaders/LineShader.shader");
		m_Data.LinePipeline = renderer->CreateGraphicsPipeline(m_Data.LineShader, PrimitiveTopology::LineStrip, layout);
		m_Data.LineVertexBuffer = renderer->CreateBuffer<VertexBuffer | StorageBuffer>(sizeof(LineVertex) * s_MaxVertices);

		m_Data.LineVertexBufferBase = new LineVertex[s_MaxVertices];
		
		uint32_t imageCount = renderer->GetSwapchain()->GetImageCount();

		m_Data.CommandBuffers.resize(imageCount);
		for (CommandBuffer& commandBuffer : m_Data.CommandBuffers)
			commandBuffer = renderer->AllocateCommandBuffer();

		m_Redraw = true;
		
		for (size_t i = 0; i < imageCount; i++)
		{
			m_RecordCommandBuffer.push_back(true);
		}

		Window& window = renderer->GetWindow();
		m_Data.LineIDBuffer = renderer->CreateBuffer<StagingBuffer>(sizeof(int));
	}

	LineRenderer::LineRenderer(Renderer* renderer, Framebuffer* framebuffer)
		: m_Renderer(renderer)
	{
		m_Data = {};

		InputLayout layout{};
		layout.VertexLayout = {
			{ ShaderDataType::Float4, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Int,    "a_LineIndex" },
			{ ShaderDataType::Int,    "a_Pad1" },
			{ ShaderDataType::Int,    "a_Pad2" },
			{ ShaderDataType::Int,    "a_Pad3" },
		};

		PushConstantInfo cameraPushConstant{};
		cameraPushConstant.Size = sizeof(glm::mat4);
		cameraPushConstant.Offset = 0;
		cameraPushConstant.Stage = ShaderStage::Vertex;

		layout.PushConstants.push_back(cameraPushConstant);

		m_Data.LineShader = renderer->CreateShader("Shaders/LineShader.shader");
		m_Data.LinePipeline = renderer->CreateGraphicsPipeline(m_Data.LineShader, PrimitiveTopology::LineStrip, layout, framebuffer);

		m_Data.LineVertexBuffer = renderer->CreateBuffer<VertexBuffer | StorageBuffer>(sizeof(LineVertex) * s_MaxVertices);
		m_Data.LineVertexBuffer->SetData(0, m_Data.LineVertexBuffer->GetSize());

		m_Data.LineDataBuffer = renderer->CreateBuffer<StorageBuffer>(sizeof(int) * 2);

		m_Data.LineVertexBufferBase = new LineVertex[s_MaxVertices];

		layout = {};
		
		ShaderResourceInfo bufferResource1{};
		bufferResource1.Binding = 0;
		bufferResource1.ResourceCount = 1;
		bufferResource1.ResourceType = ShaderResourceType::StorageBuffer;
		bufferResource1.Stage = ShaderStage::Compute;

		ShaderResourceInfo bufferResource2{};
		bufferResource2.Binding = 1;
		bufferResource2.ResourceCount = 1;
		bufferResource2.ResourceType = ShaderResourceType::StorageBuffer;
		bufferResource2.Stage = ShaderStage::Compute;

		layout.ShaderResources.push_back(bufferResource1);
		layout.ShaderResources.push_back(bufferResource2);
	
		cameraPushConstant.Stage = ShaderStage::Compute;

		layout.PushConstants.push_back(cameraPushConstant);

		m_Data.LineComputeShader = renderer->CreateShader("Shaders/LineCompute.shader");
		m_Data.LineComputePipeline = renderer->CreateComputePipeline(m_Data.LineComputeShader, layout);

		m_Data.LineComputePipeline->UpdateDescriptor(m_Data.LineVertexBuffer, 0);
		m_Data.LineComputePipeline->UpdateDescriptor(m_Data.LineDataBuffer, 1);

		uint32_t imageCount = renderer->GetSwapchain()->GetImageCount();

		m_Data.CommandBuffers.resize(imageCount);
		for (CommandBuffer& commandBuffer : m_Data.CommandBuffers)
			commandBuffer = renderer->AllocateCommandBuffer();

		m_Redraw = true;

		for (size_t i = 0; i < imageCount; i++)
		{
			m_RecordCommandBuffer.push_back(true);
		}

		m_Data.LineIDBuffer = renderer->CreateBuffer<StagingBuffer>(sizeof(int));

		m_Data.LineVertexCounts = { 2000, 2000 };
	}

	LineRenderer::~LineRenderer()
	{
		delete[] m_Data.LineVertexBufferBase;

		delete m_Data.LineIDBuffer;
		delete m_Data.LineVertexBuffer;
		delete m_Data.LineDataBuffer;
		delete m_Data.LineComputePipeline;
		delete m_Data.LineComputeShader;
		delete m_Data.LinePipeline;
		delete m_Data.LineShader;
	}

	static glm::vec4 ProjectionMinMax(const glm::mat4& proj)
	{
		std::array corners = {
			glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			glm::vec4(-1.0f,  1.0f, 0.0f, 1.0f),
			glm::vec4( 1.0f, -1.0f, 0.0f, 1.0f),
			glm::vec4( 1.0f,  1.0f, 0.0f, 1.0f)
		};

		float minX = std::numeric_limits<float>::max();
		float maxX = -std::numeric_limits<float>::max();
		float minY = std::numeric_limits<float>::max();
		float maxY = -std::numeric_limits<float>::max();

		for (const auto& corner : corners)
		{
			glm::vec4 worldPos = glm::inverse(proj) * corner;
			worldPos = glm::vec4(worldPos.x / worldPos.w, worldPos.y / worldPos.w, worldPos.z / worldPos.w, worldPos.w / worldPos.w);

			minX = std::min(minX, worldPos.x);
			maxX = std::max(maxX, worldPos.x);
			minY = std::min(minY, worldPos.y);
			maxY = std::max(maxY, worldPos.y);
		}

		return { minX, maxX, minY, maxY };
	}

	Buffer<StagingBuffer>* LineRenderer::Render(const GraphCamera& camera)
	{
		uint32_t imageIndex = m_Renderer->GetSwapchain()->GetImageIndex();
		CommandBuffer commandBuffer = m_Data.CommandBuffers[imageIndex];

		Window& window = m_Renderer->GetWindow();
		float aspect = (float)window.GetWidth() / (float)window.GetHeight();
		const glm::mat4& cameraData = camera.GetViewProjectionMatrix();

		if (m_Redraw)
		{
			m_Data.LineVertexBufferPtr = m_Data.LineVertexBufferBase;
			m_Data.LineVertexCounts.clear();

			glm::vec4 minMax = ProjectionMinMax(cameraData);

			float minX = minMax.x - 0.5f;
			float maxX = minMax.y + 0.5f;
			float step = 0.01f * (camera.GetZoomLevel() / 2.0f);

			for (int i = 0; i < m_Lines.size(); i++)
			{
				const auto& line = m_Lines[i];

				size_t& vertexCount = m_Data.LineVertexCounts.emplace_back();
				vertexCount = 0;

				for (float x = minX; x <= maxX; x += step)
				{
					m_Data.LineVertexBufferPtr->Position = { x, line.Function(x), 0.0f, 1.0f };
					m_Data.LineVertexBufferPtr->Color = line.Color;
					m_Data.LineVertexBufferPtr->LineIndex = i + 1;
					m_Data.LineVertexBufferPtr++;

					vertexCount++;
				}
			}

			size_t dataSize = (size_t)((uint8_t*)m_Data.LineVertexBufferPtr - (uint8_t*)m_Data.LineVertexBufferBase);
			m_Data.LineVertexBuffer->SetData(m_Data.LineVertexBufferBase, dataSize);

			m_Redraw = false;
		}

		Swapchain* swapchain = m_Renderer->GetSwapchain();

		m_Renderer->BeginCommandBuffer(commandBuffer);
		swapchain->BeginRenderPass(commandBuffer);

		m_Data.LinePipeline->Bind(commandBuffer);
		m_Data.LinePipeline->PushConstants(commandBuffer, ShaderStage::Vertex, sizeof(glm::mat4), &cameraData);
		m_Data.LinePipeline->SetLineWidth(commandBuffer, 10.0f);

		m_Data.LineVertexBuffer->Bind(commandBuffer);

		for (size_t i = 0; i < m_Lines.size(); i++)
			m_Renderer->Draw(commandBuffer, m_Data.LineVertexCounts[i], i == 0 ? 0 : m_Data.LineVertexCounts[i - 1]);

		swapchain->EndRenderPass(commandBuffer);
		m_Renderer->EndCommandBuffer(commandBuffer);
		m_Renderer->SubmitCommandBuffer(commandBuffer);

		m_RecordCommandBuffer[imageIndex] = false;

		return m_Data.LineIDBuffer;
	}

	Buffer<StagingBuffer>* LineRenderer::Render(const GraphCamera& camera, Framebuffer* framebuffer, const glm::vec2& relativeMousePosition)
	{
		uint32_t imageIndex = m_Renderer->GetSwapchain()->GetImageIndex();
		CommandBuffer commandBuffer = m_Data.CommandBuffers[imageIndex];

		Window& window = m_Renderer->GetWindow();
		float aspect = (float)framebuffer->GetWidth() / (float)framebuffer->GetHeight();
		const glm::mat4& cameraData = camera.GetViewProjectionMatrix();

		/*if (m_Redraw)
		{
			m_Data.LineVertexBufferPtr = m_Data.LineVertexBufferBase;
			m_Data.LineVertexCounts.clear();

			glm::vec4 minMax = ProjectionMinMax(cameraData);

			int desiredVertexCount = 2000; // per line

			float minX = minMax[0] - 0.5f;
			float maxX = minMax[1] + 0.5f;

			float range = maxX - minX;
			float step = range / (desiredVertexCount - 1);

			for (int i = 0; i < m_Lines.size(); i++)
			{
				const auto& line = m_Lines[i];

				size_t& vertexCount = m_Data.LineVertexCounts.emplace_back();
				vertexCount = 0;

				for (float x = minX; x <= maxX; x += step)
				{
					m_Data.LineVertexBufferPtr->Position = { x, line.Function(x), 0.0f };
					m_Data.LineVertexBufferPtr->Color = line.Color;
					m_Data.LineVertexBufferPtr->LineIndex = i + 1;
					m_Data.LineVertexBufferPtr++;

					vertexCount++;
				}
			}

			size_t dataSize = (size_t)((uint8_t*)m_Data.LineVertexBufferPtr - (uint8_t*)m_Data.LineVertexBufferBase);
			m_Data.LineVertexBuffer->SetData(m_Data.LineVertexBufferBase, dataSize);

			m_Redraw = false;
		}*/

		Swapchain* swapchain = m_Renderer->GetSwapchain();

		m_Renderer->BeginCommandBuffer(commandBuffer);

		m_Data.LineComputePipeline->Bind(commandBuffer);
		m_Data.LineComputePipeline->PushConstants(commandBuffer, cameraData);
		m_Data.LineComputePipeline->BindDescriptor(commandBuffer);

		m_Renderer->Dispatch(commandBuffer, 8, 1, 1);

		framebuffer->BeginRenderPass(commandBuffer);

		m_Data.LinePipeline->Bind(commandBuffer);
		m_Data.LinePipeline->PushConstants(commandBuffer, ShaderStage::Vertex, cameraData);
		m_Data.LinePipeline->SetLineWidth(commandBuffer, 10.0f);

		m_Data.LineVertexBuffer->Bind(commandBuffer);

		m_Renderer->Draw(commandBuffer, 2000, 0);
		m_Renderer->Draw(commandBuffer, 2000, 2000);

		framebuffer->EndRenderPass(commandBuffer);
		if (!(relativeMousePosition.x < 0 || relativeMousePosition.y < 0 || relativeMousePosition.x >(float)framebuffer->GetWidth() || relativeMousePosition.y >(float)framebuffer->GetHeight()))
			framebuffer->CopyAttachmentImageToBuffer(commandBuffer, 1, m_Data.LineIDBuffer, relativeMousePosition);
		m_Renderer->EndCommandBuffer(commandBuffer);
		m_Renderer->SubmitCommandBuffer(commandBuffer);

		m_RecordCommandBuffer[imageIndex] = false;

		return m_Data.LineIDBuffer;
	}

	void LineRenderer::AddLine(std::function<float(float)>&& f, const glm::vec4& color)
	{
		m_Lines.push_back({ f, color });
		m_Redraw = true;
		m_RecordCommandBuffer[m_Renderer->GetCurrentFrameIndex()] = true;
	}

	void LineRenderer::MoveCamera()
	{
		for (size_t i = 0; i < m_RecordCommandBuffer.size(); i++)
			m_RecordCommandBuffer[i] = true;
		m_Redraw = true;
	}

	bool LineRenderer::OnWindowResize(WindowResizeEvent& event)
	{
		for (size_t i = 0; i < m_RecordCommandBuffer.size(); i++)
			m_RecordCommandBuffer[i] = true;
		m_Redraw = true;

		delete m_Data.LineIDBuffer;

		m_Data.LineIDBuffer = m_Renderer->CreateBuffer<StagingBuffer>((size_t)event.GetWidth() * (size_t)event.GetHeight() * sizeof(int));

		return false;
	}

}
