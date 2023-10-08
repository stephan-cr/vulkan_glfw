// Vulkan headers need to be included before GLFW, see
// https://www.glfw.org/docs/latest/vulkan_guide.html
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

uint32_t get_instance_version()
{
  auto FN_vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

  if(FN_vkEnumerateInstanceVersion == nullptr)
    return VK_API_VERSION_1_0;
  else {
    uint32_t instanceVersion;
    const auto result = FN_vkEnumerateInstanceVersion(&instanceVersion);
    if (result != VK_SUCCESS) {
      throw std::runtime_error("Vulkan instance enumeration failed");
    }
    return instanceVersion & 0xFFFF0000; //Remove the patch version.
  }
}

class GlfwContext
{
public:
  GlfwContext()
  {
    if (!glfwInit()) {
      throw std::runtime_error("GLFW initialization failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }

  GlfwContext(const GlfwContext&) = delete;
  GlfwContext& operator=(const GlfwContext&) = delete;

  ~GlfwContext()
  {
    glfwTerminate();
  }

  bool vulkan_supported() const {
    return glfwVulkanSupported() == GLFW_TRUE;
  }

  void pool_events()
  {
    glfwPollEvents();
  }
};

class Window
{
public:
  Window(int width, int height, const char* name) :
    window{glfwCreateWindow(width, height, name, nullptr, nullptr),
           &glfwDestroyWindow} {
    if (!window) {
      throw std::runtime_error("GLFW window creation failed");
    }
  }

  void create_window_surface(VkInstance instance)
  {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window.get(), nullptr, &surface) != VK_SUCCESS) {
      throw std::runtime_error("create window surface failed");
    }
  }

  void set_key_callback(void (*callback)(GLFWwindow*, int, int, int, int))
  {
    glfwSetKeyCallback(window.get(), callback);
  }

  void make_context_current() {
    glfwMakeContextCurrent(window.get());
  }

  bool should_close() {
    return glfwWindowShouldClose(window.get());
  }

  void swap_buffers() {
    glfwSwapBuffers(window.get());
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) : window{std::exchange(other.window, nullptr)} {
  }

  Window& operator=(Window&& other) {
    window = std::move(other.window);

    return *this;
  }

private:
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window;
};

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main()
{
  std::cout << "version: " << get_instance_version() << '\n';

  try {
    GlfwContext context;

    if (!context.vulkan_supported()) {
      throw std::runtime_error("Vulkan is not supported");
    } else {
      std::cout << "Vulkan support is present\n";
    }

    uint32_t required_extensions_count;
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extensions_count);

    if (required_extensions) {
      for (uint32_t i = 0; i < required_extensions_count; ++i) {
        std::cout << required_extensions[i] << '\n';
      }
    }

    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::cout << "available extension count " << extension_count << '\n';

    std::vector<VkExtensionProperties> available_extensions{extension_count};
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    for (const auto &extension : available_extensions) {
      std::cout << '\t' << extension.extensionName << '\n';
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Sample App";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = required_extensions_count;
    create_info.ppEnabledExtensionNames = required_extensions;
    create_info.enabledLayerCount = 0;
    create_info.pNext = nullptr;

    VkInstance instance;
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
      throw std::runtime_error("Vulkan instance creation failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    Window window{640, 480, "My Title"};

    window.set_key_callback(key_callback);
    window.make_context_current();
    window.create_window_surface(instance);

    while (!window.should_close()) {
      window.swap_buffers();
      context.pool_events();
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
