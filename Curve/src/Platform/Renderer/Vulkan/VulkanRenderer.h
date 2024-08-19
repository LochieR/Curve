#pragma once

#include "Curve/Core/Base.h"
#include "Curve/Renderer/Renderer.h"

struct VkSemaphore_T; typedef VkSemaphore_T* VkSemaphore;
struct VkFence_T; typedef VkFence_T* VkFence;

namespace cv {

	struct VulkanData;

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(Window& window);
		virtual ~VulkanRenderer();

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual Window& GetWindow() override { return m_Window; }

		virtual void Draw(CommandBuffer commandBuffer, size_t vertexCount, size_t vertexOffset = 0) const override;
		virtual void DrawIndexed(CommandBuffer commandBuffer, size_t indexCount, size_t indexOffset = 0) const override;

		virtual void BeginCommandBuffer(CommandBuffer commandBuffer) const override;
		virtual void EndCommandBuffer(CommandBuffer commandBuffer) const override;
		virtual void SubmitCommandBuffer(CommandBuffer commandBuffer) const override;

		virtual Swapchain* GetSwapchain() const override;

		virtual CommandBuffer AllocateCommandBuffer() const override;
		virtual CommandBuffer BeginSingleTimeCommands() const override;
		virtual void EndSingleTimeCommands(CommandBuffer commandBuffer) const override;

		virtual Swapchain* CreateSwapchain(const SwapchainSpecification& spec) override;
		virtual Shader* CreateShader(const std::filesystem::path& path) override;
		virtual GraphicsPipeline* CreateGraphicsPipeline(Shader* shader, PrimitiveTopology topology, const InputLayout& layout) override;
		virtual VertexBuffer* CreateVertexBuffer(size_t size) override;
		virtual IndexBuffer* CreateIndexBuffer(uint32_t* indices, uint32_t indexCount) override;

		virtual uint32_t GetCurrentFrameIndex() const override;

		virtual void* GetNativeData() override { return m_VkD; }
		virtual const void* GetNativeData() const override { return m_VkD; }

		VulkanData& GetVulkanData() { return *m_VkD; }

		VkSemaphore GetNextFrameSemaphore() const;
		VkFence GetNextFrameFence() const;
		void SubmitResourceFree(std::function<void(VulkanRenderer*)>&& func);
	private:
		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateCommandPool();
		void CreateDescriptorPool();
		void CreateSyncObjects();
	private:
		Window& m_Window;
		VulkanData* m_VkD = nullptr;
	};

}

#define VK_CHECK(result, msg) CV_ASSERT(result == VK_SUCCESS && msg)
