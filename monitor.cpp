#include "graphics.hpp"

#include "GLFW/glfw3.h"

#include <cstdlib>
#include <iostream>

namespace {
  void monitor_callback(GLFWmonitor* monitor, int event)
  {
    if (event == GLFW_CONNECTED)
    {
      std::cout << "monitor connected\n";
    }
    else if (event == GLFW_DISCONNECTED)
    {
      std::cout << "monitor disconnected\n";
    }
  }
}

int main()
{
  GraphicsContext context;
  glfwSetMonitorCallback(monitor_callback);

  GLFWmonitor* primary = glfwGetPrimaryMonitor();

  int count;
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  std::cout << "monitor count: " << count << '\n';
  for (int index = 0; index < count; ++index) {
    const GLFWvidmode* modes = glfwGetVideoModes(monitors[index], &count);
    if (modes != nullptr) {
      for (int index = 0; index < count; ++index) {
        std::cout << modes[index].width << ", " << modes[index].height << '\n';
      }
    }
  }

  const char* name = glfwGetMonitorName(primary);
  std::cout << "monitor name: " << name << '\n';

  {
    int xpos, ypos, width, height;
    glfwGetMonitorWorkarea(primary, &xpos, &ypos, &width, &height);
    std::cout << "xpos: " << xpos << ", ypos: " << ypos << ", width: " << width << ", height: " << height << '\n';
  }

  {
    int xpos, ypos;
    glfwGetMonitorPos(primary, &xpos, &ypos);
    std::cout << "xpos: " << xpos << ", ypos: " << ypos << '\n';
  }

  {
    int width_mm, height_mm;
    glfwGetMonitorPhysicalSize(primary, &width_mm, &height_mm);
    std::cout << "physical size - width_mm: " << width_mm << ", height_mm: " << height_mm << '\n';
  }

  const GLFWvidmode* modes = glfwGetVideoModes(primary, &count);
  if (modes != nullptr) {
    for (int index = 0; index < count; ++index) {
      std::cout << modes[index].width << ", " << modes[index].height << '\n';
    }
  }

  return EXIT_SUCCESS;
}
