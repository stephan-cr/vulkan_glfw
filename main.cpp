// Vulkan headers need to be included before GLFW, see
// https://www.glfw.org/docs/latest/vulkan_guide.html

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_enums.hpp>
#include "vulkan/vulkan_to_string.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include "GLFW/glfw3native.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
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
    if (glfwInit() == GLFW_FALSE) {
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

  void clear()
  {
    // glClear(GL_COLOR_BUFFER_BIT);
  }

  bool vulkan_supported() const
  {
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
           &glfwDestroyWindow}
  {
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

  void make_context_current()
  {
    glfwMakeContextCurrent(window.get());
  }

  bool should_close()
  {
    return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
  }

  void show()
  {
    // glfwSetWindowPos(window.get(), 256, 256);
    // glfwShowWindow(window.get());
  }

  void swap_buffers()
  {
    glfwSwapBuffers(window.get());
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) noexcept : window{std::exchange(other.window, nullptr)}
  {
  }

  Window& operator=(Window&& other) noexcept
  {
    window = std::move(other.window);

    return *this;
  }

  GLFWwindow* raw_glfw_window() const
  {
    return window.get();
  }

  std::pair<int, int> framebuffer_size() const
  {
    int width;
    int height;
    glfwGetFramebufferSize(window.get(), &width, &height);

    return {width, height};
  }

private:
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window;
};

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void joystick_callback(int jid, int event)
{
  std::cout << "joystick event\n";
  if (event == GLFW_CONNECTED) {
    // The joystick was connected

    std::cout << "joystick connected\n";
  } else if (event == GLFW_DISCONNECTED) {
    // The joystick was disconnected

    std::cout << "joystick disconnected\n";
  }

  std::cout.flush();
}

int main()
{
  std::cout << "version: " << get_instance_version() << '\n';

  try {
    GlfwContext context;
    glfwSetJoystickCallback(joystick_callback);

    if (!context.vulkan_supported()) {
      throw std::runtime_error("Vulkan is not supported");
    } else {
      std::cout << "Vulkan support is present\n";
    }

    uint32_t required_extensions_count;
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extensions_count);

    if (required_extensions != nullptr) {
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

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[0], &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[0], &queue_family_count, queue_families.data());

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
      const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      };

      VkDeviceCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      create_info.pQueueCreateInfos = &queue_create_info;
      create_info.queueCreateInfoCount = 1;
      create_info.pEnabledFeatures = &device_features;
      create_info.enabledExtensionCount = device_extensions.size();
      create_info.ppEnabledExtensionNames = device_extensions.data();
      create_info.enabledLayerCount = 0;

      VkDevice temp_device;
      if (vkCreateDevice(physical_devices[0], &create_info, nullptr, &temp_device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
      }

      device.reset(temp_device);
    }

    VkQueue graphics_queue;

    vkGetDeviceQueue(device.get(), queue_family_index.value(), 0, &graphics_queue);

    Window window{640, 480, "My Title"};

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Window_surface
    VkSurfaceKHR surface;
    {
      VkWaylandSurfaceCreateInfoKHR create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      create_info.display = glfwGetWaylandDisplay();
      create_info.surface = glfwGetWaylandWindow(window.raw_glfw_window()); // TODO how to abstract that well

      if (vkCreateWaylandSurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
      }
    }

    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[0], queue_family_index.value(), surface, &present_support);
    std::cout << std::boolalpha << "present_support: " << present_support << '\n';

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window.set_key_callback(key_callback);
    window.make_context_current();
    window.create_window_surface(instance, surface);

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain
    {
      uint32_t extension_count;
      vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr, &extension_count, nullptr);

      std::vector<VkExtensionProperties> available_extensions(extension_count);
      vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr, &extension_count, available_extensions.data());

      std::set<std::string> available_extension_set;
      for (const auto& extension: available_extensions) {

        available_extension_set.insert(extension.extensionName);
      }

      for (const auto& extension: available_extension_set) {
        std::cout << extension << '\n';
      }

      struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
      };

      SwapChainSupportDetails details;

      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_devices[0], surface, &details.capabilities);
      int window_width, window_height;
      std::tie(window_width, window_height) = window.framebuffer_size();
      std::cout << "currentExtent.height: " << details.capabilities.currentExtent.height <<
        " currentExtent.width: " << details.capabilities.currentExtent.width <<
        " window_height: " << window_height << " window width: " << window_width <<
        '\n';

      VkExtent2D actual_extent = {
        static_cast<uint32_t>(window_width),
        static_cast<uint32_t>(window_height)
      };

      actual_extent.width = std::clamp(actual_extent.width,
                                      details.capabilities.minImageExtent.width,
                                      details.capabilities.maxImageExtent.width);
      actual_extent.height = std::clamp(actual_extent.height,
                                       details.capabilities.minImageExtent.height,
                                       details.capabilities.maxImageExtent.height);

      uint32_t format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[0], surface, &format_count, nullptr);

      if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[0],
                                             surface,
                                             &format_count,
                                             details.formats.data());
      }

      uint32_t present_mode_count;
      vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[0], surface, &present_mode_count, nullptr);

      if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[0],
                                                  surface,
                                                  &present_mode_count,
                                                  details.present_modes.data());
      }

      std::cout << "format_count: " << format_count << " present_mode_count: " << present_mode_count << '\n';

      for (const auto& format : details.formats) {
        const vk::SurfaceFormatKHR o(format);

        std::cout << vk::to_string(o.format) << " : " << vk::to_string(o.colorSpace) << '\n';
      }

      for (const auto& present_mode : details.present_modes) {
        const vk::PresentModeKHR o = static_cast<vk::PresentModeKHR>(present_mode);

        std::cout << "present mode: " << vk::to_string(o) << '\n';
      }

      const uint32_t image_count = details.capabilities.minImageCount + 1;
      assert(image_count <= details.capabilities.maxImageCount);

      VkSwapchainCreateInfoKHR create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      create_info.surface = surface;
      create_info.minImageCount = image_count;
      create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
      create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      create_info.imageExtent = actual_extent;
      create_info.imageArrayLayers = 1;
      create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

      // TODO fill in the rest of the create_info configuration items

      std::unique_ptr<std::remove_pointer_t<VkSwapchainKHR>, std::function<void(VkSwapchainKHR)>>
        swap_chain{
        nullptr,
        [&device](VkSwapchainKHR chain){
          vkDestroySwapchainKHR(device.get(), chain, nullptr);
        }};

      {
        VkSwapchainKHR temp_swap_chain;
        if (vkCreateSwapchainKHR(device.get(), &create_info, nullptr, &temp_swap_chain) != VK_SUCCESS) {
          throw std::runtime_error("failed to create swap chain!");
        }

        swap_chain.reset(temp_swap_chain);
      }
    }

    window.show();
    while (!window.should_close()) {
      context.clear();
      window.swap_buffers();
      context.pool_events();
    }

    vkDeviceWaitIdle(device.get());
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
