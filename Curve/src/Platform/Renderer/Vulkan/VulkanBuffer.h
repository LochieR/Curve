#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Buffer.h"

namespace cv {

	struct BufferData;

	class VulkanBuffer : public BufferBase
	{
	public:
		VulkanBuffer(VulkanRenderer* renderer, BufferType type, size_t size, const void* data = nullptr);
		virtual ~VulkanBuffer();

		virtual void Bind(CommandBuffer commandBuffer) const override;

		virtual void SetData(const void* data, size_t size) override;
		virtual void SetData(int data, size_t size) override;

		virtual void* Map(size_t size) override;
		virtual void Unmap() override;

		virtual size_t GetSize() const override;

		virtual void* GetNativeData() override { return m_Data; }
		virtual const void* GetNativeData() const override { return m_Data; }
	private:
		VulkanRenderer* m_Renderer = nullptr;
		BufferData* m_Data = nullptr;
		BufferData* m_StagingData = nullptr;

		BufferType m_Type;
	};

}
