#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Buffer.h"

namespace cv {

	struct BufferData;

	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(VulkanRenderer* renderer, size_t size);
		virtual ~VulkanVertexBuffer();

		virtual void Bind(CommandBuffer commandBuffer) const override;

		virtual void SetData(const void* vertexData, uint32_t size) override;

		virtual const InputLayout& GetLayout() const override { return m_Layout; }
		virtual void SetLayout(const InputLayout& layout) override { m_Layout = layout; }
	private:
		VulkanRenderer* m_Renderer = nullptr;
		InputLayout m_Layout;
		BufferData* m_Data = nullptr;
	};

	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(VulkanRenderer* renderer, uint32_t* indices, uint32_t indexCount);
		virtual ~VulkanIndexBuffer();

		virtual void Bind(CommandBuffer commandBuffer) const override;
	private:
		VulkanRenderer* m_Renderer = nullptr;
		BufferData* m_Data = nullptr;
	};

}
