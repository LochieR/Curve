VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}

IncludeDir["GLFW"] = "%{wks.location}/Curve/vendor/GLFW/include"
IncludeDir["glm"] = "%{wks.location}/Curve/vendor/glm"
IncludeDir["imgui"] = "%{wks.location}/Curve/vendor/imgui"
IncludeDir["stb_image"] = "%{wks.location}/Curve/vendor/stb_image"
IncludeDir["Vulkan"] = "%{VULKAN_SDK}/Include"

LibraryDir = {}

LibraryDir["Vulkan"] = "%{VULKAN_SDK}/Lib"

Library = {}

Library["Vulkan"] = "%{LibraryDir.Vulkan}/vulkan-1.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.Vulkan}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.Vulkan}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.Vulkan}/spirv-cross-glsld.lib"

Library["ShaderC_Release"] = "%{LibraryDir.Vulkan}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.Vulkan}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.Vulkan}/spirv-cross-glsl.lib"

workspace "Curve"
	architecture "x86_64"
	startproject "View"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags { "MultiProcessorCompile" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "Curve/vendor/GLFW"
	include "Curve/vendor/ImGui"
group ""

project "Curve"
	location "Curve"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "cvpch.h"
	pchsource "%{prj.location}/src/cvpch.cpp"

	files
	{
		"%{prj.location}/src/**.h",
		"%{prj.location}/src/**.cpp",
		"%{prj.location}/deps/stb_image/**.h",
		"%{prj.location}/deps/stb_image/**.cpp",
		"%{prj.location}/deps/glm/glm/**.hpp",
		"%{prj.location}/deps/glm/glm/**.inl",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

	includedirs
	{
		"%{prj.location}/src"
	}

	externalincludedirs
	{
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.Vulkan}",
	}

	links
	{
		"GLFW",
		"ImGui",
		"%{Library.Vulkan}"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "CV_DEBUG"
		runtime "Debug"
		symbols "on"

		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}"
		}

	filter "configurations:Release"
		defines "CV_RELEASE"
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}

	filter "configurations:Dist"
		defines "CV_DIST"
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}

project "View"
	location "View"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.location}/src/**.h",
		"%{prj.location}/src/**.cpp"
	}

	includedirs
	{
		"%{wks.location}/Curve/src"
	}

	externalincludedirs
	{
		"%{IncludeDir.glm}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.Vulkan}"
	}

	links
	{
		"Curve"
	}

	filter "system:windows"
		systemversion "latest"

		defines { "NOMINMAX" }

	filter "configurations:Debug"
		kind "ConsoleApp"
		defines "CV_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		kind "ConsoleApp"
		defines "CV_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		kind "WindowedApp"
		defines "CV_DIST"
		runtime "Release"
		optimize "on"
