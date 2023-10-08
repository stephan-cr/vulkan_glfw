// Vulkan headers need to be included before GLFW, see
// https://www.glfw.org/docs/latest/vulkan_guide.html
#define VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include "GLFW/glfw3native.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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

  void create_window_surface(VkInstance instance, VkSurfaceKHR surface)
  {
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

  void show() {
    glfwSetWindowPos(window.get(), 256, 256);
    glfwShowWindow(window.get());
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

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::cout << "device count: " << device_count << '\n';

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_family_count, queue_families.data());

    std::cout << "queue family count: " << queue_family_count << '\n';

    std::optional<uint32_t> queue_family_index;
    uint32_t index = 0;
    for (const auto& queue_family : queue_families) {
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queue_family_index = index;
        break;
      }

      ++index;
    }

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Logical_device_and_queues
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family_index.value();
    queue_create_info.queueCount = 1;
    const float queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features{};

    std::unique_ptr<std::remove_pointer_t<VkDevice>, void (*)(VkDevice)> device{nullptr, [](VkDevice device) { vkDestroyDevice(device, nullptr); }};
    {
      VkDeviceCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      create_info.pQueueCreateInfos = &queue_create_info;
      create_info.queueCreateInfoCount = 1;
      create_info.pEnabledFeatures = &device_features;
      create_info.enabledExtensionCount = 0;
      create_info.enabledLayerCount = 0;

      VkDevice temp_device;
      if (vkCreateDevice(devices[0], &create_info, nullptr, &temp_device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
      }

      device.reset(temp_device);
    }

    VkQueue graphics_queue;

    vkGetDeviceQueue(device.get(), queue_family_index.value(), 0, &graphics_queue);

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Window_surface
    VkSurfaceKHR surface;
    {
      VkWaylandSurfaceCreateInfoKHR create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      create_info.display = glfwGetWaylandDisplay();
      create_info.surface = nullptr;

      if (vkCreateWaylandSurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
      }
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    Window window{640, 480, "My Title"};

    window.set_key_callback(key_callback);
    window.make_context_current();
    window.create_window_surface(instance, surface);

    window.show();
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
