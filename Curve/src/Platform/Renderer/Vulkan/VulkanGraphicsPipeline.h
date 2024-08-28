#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Shader.h"
#include "Curve/Renderer/GraphicsPipeline.h"

namespace cv {

	struct PipelineData;

	class VulkanGraphicsPipeline : public GraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(VulkanRenderer* renderer, Shader* shader, PrimitiveTopology topology, const InputLayout& layout);
		VulkanGraphicsPipeline(VulkanRenderer* renderer, Shader* shader, PrimitiveTopology topology, const InputLayout& layout, Framebuffer* framebuffer);
		virtual ~VulkanGraphicsPipeline();

		virtual void Bind(CommandBuffer commandBuffer) const override;
		virtual void PushConstants(CommandBuffer commandBuffer, ShaderStage shaderStage, size_t size, const void* data, size_t offset = 0) override;
		virtual void SetLineWidth(CommandBuffer commandBuffer, float lineWidth) override;

		virtual void* GetNativeData() override { return m_Data; }
		virtual const void* GetNativeData() const override { return m_Data; }
	private:
		VulkanRenderer* m_Renderer = nullptr;
		PipelineData* m_Data = nullptr;
		Framebuffer* m_Framebuffer = nullptr;
	};

}
