#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Framebuffer.h"

#include <vector>

enum VkSampleCountFlagBits;
struct VkRenderPass_T; typedef VkRenderPass_T* VkRenderPass;

namespace cv {

	struct FramebufferData;

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(VulkanRenderer* renderer, const FramebufferSpecification& spec);
		virtual ~VulkanFramebuffer();

		virtual void BeginRenderPass(CommandBuffer commandBuffer) override;
		virtual void EndRenderPass(CommandBuffer commandBuffer) override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetColorAttachmentCount() const override;

		//virtual void CopyAttachmentImageToBuffer(uint32_t attachmentIndex) override;

		virtual void* GetCurrentDescriptor() const override;

		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }

		virtual bool IsMultisampled() const override { return m_Specification.Multisample; }

		VkRenderPass GetRenderPass() const;

		virtual void* GetNativeData() override { return m_Data; }
		virtual const void* GetNativeData() const override { return m_Data; }
	private:
		VulkanRenderer* m_Renderer = nullptr;
		FramebufferSpecification m_Specification;

		FramebufferData* m_Data;
	};

}
