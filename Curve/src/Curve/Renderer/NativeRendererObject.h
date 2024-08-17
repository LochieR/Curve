#pragma once

namespace cv {

	class NativeRendererObject
	{
	public:
		virtual ~NativeRendererObject() = default;

		virtual void* GetNativeData() = 0;
		virtual const void* GetNativeData() const = 0;
	};

}

#define CV_NATIVE_DATA_TEMPLATE() \
	template<typename T> T& GetNativeData() { return *reinterpret_cast<T*>(GetNativeData()); } \
	template<typename T> const T& GetNativeData() const { return *reinterpret_cast<const T*>(GetNativeData()); }

