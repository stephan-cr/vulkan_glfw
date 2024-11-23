#include "graphics.hpp"

#include "GLFW/glfw3.h"

#include <stdexcept>

GraphicsContext::GraphicsContext(ClientAPI client_api)
{
  if (glfwInit() == GLFW_FALSE) {
    throw std::runtime_error("GLFW initialization failed");
  }

  switch (client_api) {
  case ClientAPI::OPENGL:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    break;
  case ClientAPI::OPENGL_ES:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    break;
  default:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
}

GraphicsContext::~GraphicsContext()
{
  glfwTerminate();
}

void GraphicsContext::clear()
{
  // glClear(GL_COLOR_BUFFER_BIT);
}

bool GraphicsContext::vulkan_supported() const
{
  return glfwVulkanSupported() == GLFW_TRUE;
}

void GraphicsContext::pool_events()
{
  glfwPollEvents();
}

void GraphicsContext::set_window_floating_hint(bool floating)
{
  glfwWindowHint(GLFW_FLOATING, floating ? GLFW_TRUE : GLFW_FALSE);
}

double GraphicsContext::time()
{
  return glfwGetTime();
}

Window::Window(int width, int height, const char* name) :
    window{glfwCreateWindow(width, height, name, nullptr, nullptr),
           &glfwDestroyWindow}
{
  if (!window) {
    throw std::runtime_error("GLFW window creation failed");
  }
}

VkSurfaceKHR Window::create_window_surface(VkInstance instance)
{
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(instance, window.get(), nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("create window surface failed");
  }

  return surface;
}

void Window::set_key_callback(void (*callback)(GLFWwindow*, int, int, int, int))
{
  glfwSetKeyCallback(window.get(), callback);
}

[[deprecated]]
void Window::make_context_current()
{
  glfwMakeContextCurrent(window.get());
}

bool Window::should_close()
{
  return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
}

void Window::show()
{
  // glfwSetWindowPos(window.get(), 256, 256);
  // glfwShowWindow(window.get());
}

[[deprecated]]
void Window::swap_buffers()
{
  glfwSwapBuffers(window.get());
}

Window::Window(Window&& other) noexcept : window{std::exchange(other.window, nullptr)}
{
}

Window& Window::operator=(Window&& other) noexcept
{
  window = std::move(other.window);

  return *this;
}

GLFWwindow* Window::raw_glfw_window() const
{
  return window.get();
}

std::pair<int, int> Window::framebuffer_size() const
{
  int width;
  int height;
  glfwGetFramebufferSize(window.get(), &width, &height);

  return {width, height};
}
