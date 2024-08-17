#pragma once

#include "NativeRendererObject.h"

#include <type_traits>

namespace cv {

	class CommandBuffer : public NativeRendererObject
	{
	public:
		CommandBuffer() = default;
		CommandBuffer(void* commandBuffer)
			: m_CommandBuffer(commandBuffer)
		{
		}
		virtual ~CommandBuffer() = default;

		virtual void* GetNativeData() override { return m_CommandBuffer; }
		virtual const void* GetNativeData() const override { return m_CommandBuffer; }

		template<typename T>
		std::enable_if_t<std::is_pointer<T>::value, T> As()
		{
			return reinterpret_cast<T>(m_CommandBuffer);
		}
	private:
		void* m_CommandBuffer = nullptr;
	};

}
