#include "cvpch.h"
#include "Time.h"

#include <GLFW/glfw3.h>

namespace cv {

    float Time::GetTime()
    {
        return (float)glfwGetTime();
    }

}
