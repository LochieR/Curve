#pragma once

#include "Swapchain.h"
#include "CommandBuffer.h"

namespace cv {

	struct FramebufferSpecification
	{
		uint32_t Width = 1, Height = 1;
		std::vector<AttachmentFormat> Attachments;
		bool Multisample = false;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void BeginRenderPass(CommandBuffer commandBuffer) = 0;
		virtual void EndRenderPass(CommandBuffer commandBuffer) = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual uint32_t GetColorAttachmentCount() const = 0;

		//virtual void CopyAttachmentImageToBuffer(uint32_t attachmentIndex) = 0;

		virtual void* GetCurrentDescriptor() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual bool IsMultisampled() const = 0;

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<const T*>(GetNativeData()); }

		virtual void* GetNativeData() = 0;
		virtual const void* GetNativeData() const = 0;
	};

}
