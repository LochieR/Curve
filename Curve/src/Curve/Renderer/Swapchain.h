#pragma once

#include "CommandBuffer.h"
#include "NativeRendererObject.h"

#include <vector>

namespace cv {

	enum class AttachmentFormat
	{
		Default = 0,
		RGBA32F,
		R32SInt, R32UInt,

		Depth
	};

	struct SwapchainSpecification
	{
		std::vector<AttachmentFormat> Attachments;
		bool Multisample = true;
	};

	class Swapchain : public NativeRendererObject
	{
	public:
		virtual ~Swapchain() = default;

		virtual bool AcquireNextImage(uint32_t& imageIndex) = 0;

		virtual void BeginRenderPass(CommandBuffer commandBuffer) const = 0;
		virtual void EndRenderPass(CommandBuffer commandBuffer) const = 0;

		virtual uint32_t GetImageCount() const = 0;
		virtual uint32_t GetImageIndex() const = 0;

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(((NativeRendererObject*)this)->GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<const T*>(((NativeRendererObject*)this)->GetNativeData()); }
	};

}
