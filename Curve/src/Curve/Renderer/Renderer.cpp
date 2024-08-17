#include "cvpch.h"
#include "Renderer.h"

#include "Platform/Renderer/Vulkan/VulkanRenderer.h"

namespace cv {

    Renderer* Renderer::Create(Window& window)
    {
        return new VulkanRenderer(window);
    }

}
