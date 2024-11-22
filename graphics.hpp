#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/vulkan_core.h"

#include "GLFW/glfw3.h"

#include <memory>
#include <utility>

class GraphicsContext
{
public:
  GraphicsContext();
  GraphicsContext(const GraphicsContext&) = delete;
  GraphicsContext& operator=(const GraphicsContext&) = delete;
  ~GraphicsContext();

  void clear();
  bool vulkan_supported() const;
  void pool_events();
  void set_window_floating_hint(bool floating);
  double time();
};

class Window
{
public:
  Window(int width, int height, const char* name);

  VkSurfaceKHR create_window_surface(VkInstance instance);

  void set_key_callback(void (*callback)(GLFWwindow*, int, int, int, int));

  [[deprecated]]
  void make_context_current();

  bool should_close();
  void show();

  [[deprecated]]
  void swap_buffers();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) noexcept;
  Window& operator=(Window&& other) noexcept;

  GLFWwindow* raw_glfw_window() const;
  std::pair<int, int> framebuffer_size() const;

private:
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window;
};


#endif // GRAPHICS_HPP