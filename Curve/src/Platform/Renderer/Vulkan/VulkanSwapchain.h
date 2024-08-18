#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Swapchain.h"

namespace cv {

	struct SwapchainData;

	class VulkanSwapchain : public Swapchain
	{
	public:
		VulkanSwapchain(VulkanRenderer* renderer, const SwapchainSpecification& spec);
		virtual ~VulkanSwapchain();

		virtual bool AcquireNextImage(uint32_t& imageIndex) override;

		virtual void BeginRenderPass(CommandBuffer commandBuffer) const override;
		virtual void EndRenderPass(CommandBuffer commandBuffer) const override;

		virtual uint32_t GetImageCount() const override;
		virtual uint32_t GetImageIndex() const override;

		virtual void* GetNativeData() override { return m_Data; }
		virtual const void* GetNativeData() const override { return m_Data; }

		void RecreateSwapchain(void* semaphore = nullptr);
	private:
		void CreateSwapchain(void* oldSwapchain = nullptr);
		void CreateRenderPass();
		void CreateColorResources();
		void CreateFramebuffers();
	private:
		VulkanRenderer* m_Renderer = nullptr;
		SwapchainSpecification m_Specification;
		SwapchainData* m_Data = nullptr;
	};

}
