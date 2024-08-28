#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/ComputePipeline.h"

namespace cv {

	struct PipelineData;

	class VulkanComputePipeline : public ComputePipeline
	{
	public:
		VulkanComputePipeline(VulkanRenderer* renderer, Shader* shader, const InputLayout& layout);
		virtual ~VulkanComputePipeline();

		virtual void Bind(CommandBuffer commandBuffer) const override;
		virtual void PushConstants(CommandBuffer commandBuffer, size_t size, const void* data, size_t offset = 0) override;

		virtual void UpdateDescriptor(BufferBase* buffer, uint32_t binding, uint32_t index) override;
		virtual void BindDescriptor(CommandBuffer commandBuffer) const override;
	private:
		VulkanRenderer* m_Renderer = nullptr;
		PipelineData* m_Data = nullptr;
	};

}
