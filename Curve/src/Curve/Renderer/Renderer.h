#pragma once

#include "Buffer.h"
#include "Shader.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "CommandBuffer.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"
#include "NativeRendererObject.h"

#include "Curve/ImGui/ImGuiLayer.h"

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

		virtual void Draw(CommandBuffer commandBuffer, size_t vertexCount, size_t vertexOffset = 0) const = 0;
		virtual void DrawIndexed(CommandBuffer commandBuffer, size_t indexCount, size_t indexOffset = 0) const = 0;

		virtual void Dispatch(CommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const = 0;

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
		virtual GraphicsPipeline* CreateGraphicsPipeline(Shader* shader, PrimitiveTopology topology, const InputLayout& layout, Framebuffer* framebuffer) = 0;
		virtual ComputePipeline* CreateComputePipeline(Shader* shader, const InputLayout& layout) = 0;
		virtual Framebuffer* CreateFramebuffer(const FramebufferSpecification& spec) = 0;
		virtual ImGuiLayer* CreateImGuiLayer() = 0;

		template<BufferType Type>
		Buffer<Type>* CreateBuffer(size_t size, const void* data = nullptr)
		{
			return new Buffer<Type>(CreateBufferBase(Type, size, data));
		}

		virtual uint32_t GetCurrentFrameIndex() const = 0;

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<const T*>(GetNativeData()); }
	private:
		virtual BufferBase* CreateBufferBase(BufferType type, size_t size, const void* data = nullptr) = 0;
	};

}
