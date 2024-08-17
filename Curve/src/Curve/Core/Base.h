#pragma once

// Platform detection using predefined macros
#ifdef _WIN32
	/* Windows x64/x86 */
	#ifdef _WIN64
		/* Windows x64  */
		#define CV_PLATFORM_WINDOWS
	#else
		/* Windows x86 */
		#error "x86 Builds are not supported!"
	#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	/* TARGET_OS_MAC exists on all the platforms
	 * so we must check all of them (in this order)
	 * to ensure that we're running on MAC
	 * and not some other Apple platform */
	#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported!"
	#elif TARGET_OS_IPHONE == 1
		#define CV_PLATFORM_IOS
		#error "IOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#define CV_PLATFORM_MACOSX
		#error "MacOS is not supported!"
	#else
		#error "Unknown Apple platform!"
	#endif
/* We also have to check __ANDROID__ before __linux__
 * since android is based on the linux kernel
 * it has __linux__ defined */
#elif defined(__ANDROID__)
	#define CV_PLATFORM_ANDROID
	#error "Android is not supported!"
#elif defined(__linux__)
	#define CV_PLATFORM_LINUX
	#error "Linux is not supported!"
#else
	/* Unknown compiler/platform */
	#error "Unknown platform!"
#endif // End of platform detection

#ifdef CV_DEBUG
	#if defined(CV_PLATFORM_WINDOWS)
		#define CV_DEBUGBREAK() __debugbreak()
	#elif defined(CV_PLATFORM_LINUX) || defined(CV_PLATFORM_MACOSX)
		#include <signal.h>
		#define CV_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define CV_ENABLE_ASSERTS
#else
	#define CV_DEBUGBREAK()
#endif

#include <iostream>

#include "Log.h"

#define CV_ASSERT(x) { if (!(x)) { std::cerr << "Assertion failed: " #x << std::endl; CV_DEBUGBREAK(); } }

#define CV_FRAMES_IN_FLIGHT 2
#define CV_MAX_SUBMITS_PER_FRAME 12
