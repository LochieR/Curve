#pragma once

#include "Buffer.h"
#include "Shader.h"
#include "Swapchain.h"
#include "CommandBuffer.h"
#include "GraphicsPipeline.h"
#include "NativeRendererObject.h"

#include "Curve/Core/Window.h"

#include <filesystem>

namespace cv {

	class Renderer : public NativeRendererObject
	{
	public:
		virtual ~Renderer() = default;

		static Renderer* Create(Window& window);

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual Window& GetWindow() = 0;

		virtual void Draw(CommandBuffer commandBuffer, size_t vertexCount) const = 0;
		virtual void DrawIndexed(CommandBuffer commandBuffer, size_t indexCount) const = 0;

		virtual void BeginCommandBuffer(CommandBuffer commandBuffer) const = 0;
		virtual void EndCommandBuffer(CommandBuffer commandBuffer) const = 0;
		virtual void SubmitCommandBuffer(CommandBuffer commandBuffer) const = 0;

		virtual Swapchain* GetSwapchain() const = 0;

		virtual CommandBuffer AllocateCommandBuffer() const = 0;
		virtual CommandBuffer BeginSingleTimeCommands() const = 0;
		virtual void EndSingleTimeCommands(CommandBuffer commandBuffer) const = 0;

		virtual Swapchain* CreateSwapchain(const SwapchainSpecification& spec) = 0;
		virtual Shader* CreateShader(const std::filesystem::path& path) = 0;
		virtual GraphicsPipeline* CreateGraphicsPipeline(Shader* shader, PrimitiveTopology topology, const InputLayout& layout) = 0;
		virtual VertexBuffer* CreateVertexBuffer(size_t size) = 0;
		virtual IndexBuffer* CreateIndexBuffer(uint32_t* indices, uint32_t indexCount) = 0;

		virtual uint32_t GetCurrentFrameIndex() const = 0;

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<const T*>(GetNativeData()); }
	};

}
