// Vulkan headers need to be included before GLFW, see
// https://www.glfw.org/docs/latest/vulkan_guide.html

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "vulkan/vulkan.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_to_string.hpp"

#include "GLFW/glfw3.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include "cxxopts.hpp"

#include "allocator.hpp"
#include "executable_info.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
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
    uint32_t instanceVersion = ~0U;
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

  void set_window_floating_hint(bool floating)
  {
    glfwWindowHint(GLFW_FLOATING, floating ? GLFW_TRUE : GLFW_FALSE);
  }

  double time()
  {
    return glfwGetTime();
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

  VkSurfaceKHR create_window_surface(VkInstance instance)
  {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window.get(), nullptr, &surface) != VK_SUCCESS) {
      throw std::runtime_error("create window surface failed");
    }

    return surface;
  }

  void set_key_callback(void (*callback)(GLFWwindow*, int, int, int, int))
  {
    glfwSetKeyCallback(window.get(), callback);
  }

  [[deprecated]]
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

  [[deprecated]]
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

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static VkVertexInputBindingDescription getBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
  {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
  }
};

const std::vector<Vertex> vertices = {
  {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
  {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

static void error_callback(int code, const char* description)
{
  std::cerr << "error: " << code << ", " << description << '\n';
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void joystick_callback(int jid, int event)
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

static void record_command_buffer(std::remove_pointer_t<VkCommandBuffer> &command_buffer,
                                  std::remove_pointer_t<VkPipeline> &graphics_pipeline,
                                  std::remove_pointer_t<VkRenderPass> &render_pass,
                                  std::remove_pointer_t<VkFramebuffer> &swap_chain_framebuffer,
                                  VkExtent2D &actual_extent,
                                  VkBuffer vertex_buffer)
{
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = 0; // Optional
  begin_info.pInheritanceInfo = nullptr; // Optional

  if (vkBeginCommandBuffer(&command_buffer, &begin_info) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = &render_pass;
  render_pass_info.framebuffer = &swap_chain_framebuffer;
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = actual_extent;

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clearColor;

  vkCmdBeginRenderPass(&command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(&command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, &graphics_pipeline);
  VkBuffer vertexBuffers[] = {vertex_buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(&command_buffer, 0, 1, vertexBuffers, offsets);

  {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(actual_extent.width);
    viewport.height = static_cast<float>(actual_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(&command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = actual_extent;
    vkCmdSetScissor(&command_buffer, 0, 1, &scissor);
    vkCmdDraw(&command_buffer, 3, 1, 0, 0);
  }

  vkCmdEndRenderPass(&command_buffer);
  if (vkEndCommandBuffer(&command_buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

int main(int argc, char** argv)
{
  const auto executable_dir = get_executable_path().parent_path();
  cxxopts::Options options(argv[0], "Vulkan tutorial");
  options.add_options()
    // ("b,bar", "Param bar", cxxopts::value<std::string>())
    ("w,width", "window width", cxxopts::value<int>()->default_value("640"))
    ("x,height", "window height", cxxopts::value<int>()->default_value("480"))
    ("d,debug", "Enable debugging", cxxopts::value<bool>()->default_value("false"))
    ("h,help", "Print usage");
  const auto parse_result = options.parse(argc, argv);

  if (parse_result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  std::cout << "version: " << get_instance_version() << '\n';

  try {
    glfwSetErrorCallback(error_callback);
    GlfwContext context;
    context.set_window_floating_hint(true);
    glfwSetJoystickCallback(joystick_callback);

    if (!context.vulkan_supported()) {
      throw std::runtime_error("Vulkan is not supported");
    } else {
      std::cout << "Vulkan support is present\n";
    }

    uint32_t required_extensions_count;
    const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_extensions_count);

    std::cout << "required extensions:\n";
    if (required_extensions != nullptr) {
      for (uint32_t i = 0; i < required_extensions_count; ++i) {
        std::cout << '\t' << required_extensions[i] << '\n';
      }
    }

    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions{extension_count};
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    std::cout << "available extensions:\n";
    std::cout << "\tavailable extension count " << extension_count << '\n';
    for (const auto &extension : available_extensions) {
      std::cout << '\t' << extension.extensionName << '\n';
    }

    std::cout << "layers:\n";
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::cout << "\tlayer count " << layer_count << '\n';

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    for (const auto &layer : available_layers) {
      std::cout << '\t' << layer.layerName << '\n';
    }

    const std::array<const char*, 1> validation_layers{
      "VK_LAYER_KHRONOS_validation"
        };
    auto validation_layer_found = std::find_if(std::begin(available_layers), std::end(available_layers),
                                               [](auto& layer) {
                                                 return strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
                                               });
    if (validation_layer_found == available_layers.end())
      throw std::runtime_error("validation layer not available");

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
    if (parse_result["debug"].as<bool>()) {
      create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
      create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
      create_info.enabledLayerCount = 0;
    }
    create_info.pNext = nullptr;

    std::unique_ptr<std::remove_pointer_t<VkInstance>, void (*)(VkInstance)> instance{
      nullptr,
      [](VkInstance instance) { vkDestroyInstance(instance, nullptr); }
    };

    {
      VkInstance temp_instance;
      if (vkCreateInstance(&create_info, nullptr, &temp_instance) != VK_SUCCESS) {
        throw std::runtime_error("Vulkan instance creation failed");
      }

      instance.reset(temp_instance);
    }

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance.get(), &device_count, nullptr);
    std::cout << "device count: " << device_count << '\n';

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance.get(), &device_count, physical_devices.data());

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

    std::unique_ptr<std::remove_pointer_t<VkDevice>, void (*)(VkDevice)>
      device{nullptr, [](VkDevice device) { vkDestroyDevice(device, nullptr); }};
    {
      const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      };

      VkDeviceCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      create_info.pQueueCreateInfos = &queue_create_info;
      create_info.queueCreateInfoCount = 1;
      create_info.pEnabledFeatures = &device_features;
      create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
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

    Window window{parse_result["width"].as<int>(), parse_result["height"].as<int>(), "Vulkan"};

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Window_surface
    std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, std::function<void(VkSurfaceKHR)>> surface{
      window.create_window_surface(instance.get()),
      [&instance](VkSurfaceKHR surface) { vkDestroySurfaceKHR(instance.get(), surface, nullptr); }
    };

    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[0], queue_family_index.value(), surface.get(), &present_support);
    std::cout << std::boolalpha << "present_support: " << present_support << '\n';

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window.set_key_callback(key_callback);

    // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain
    std::unique_ptr<std::remove_pointer_t<VkSwapchainKHR>, std::function<void(VkSwapchainKHR)>>
      swap_chain{
      nullptr,
      [&device](VkSwapchainKHR chain){
        vkDestroySwapchainKHR(device.get(), chain, nullptr);
      }};
    struct SwapChainSupportDetails {
      VkSurfaceCapabilitiesKHR capabilities;
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR> present_modes;
    };
    SwapChainSupportDetails details;
    VkExtent2D actual_extent{};
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
        std::cout << '\t' << extension << '\n';
      }

      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_devices[0], surface.get(), &details.capabilities);
      int window_width, window_height;
      std::tie(window_width, window_height) = window.framebuffer_size();
      std::cout << "currentExtent.height: " << details.capabilities.currentExtent.height <<
        " currentExtent.width: " << details.capabilities.currentExtent.width <<
        " window_height: " << window_height << " window width: " << window_width <<
        '\n';

      actual_extent.width = std::clamp(static_cast<uint32_t>(window_width),
                                       details.capabilities.minImageExtent.width,
                                       details.capabilities.maxImageExtent.width);
      actual_extent.height = std::clamp(static_cast<uint32_t>(window_height),
                                        details.capabilities.minImageExtent.height,
                                        details.capabilities.maxImageExtent.height);

      uint32_t format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[0], surface.get(), &format_count, nullptr);

      if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[0],
                                             surface.get(),
                                             &format_count,
                                             details.formats.data());
      }

      uint32_t present_mode_count;
      vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[0], surface.get(), &present_mode_count, nullptr);

      if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[0],
                                                  surface.get(),
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
      create_info.surface = surface.get();
      create_info.minImageCount = image_count;
      create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
      create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      create_info.imageExtent = actual_extent;
      create_info.imageArrayLayers = 1;
      create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      if (details.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
      else
        throw std::runtime_error("VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR not supported");
      if (details.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      else
        throw std::runtime_error("VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR not supported");

      {
        VkSwapchainKHR temp_swap_chain;
        if (vkCreateSwapchainKHR(device.get(), &create_info, nullptr, &temp_swap_chain) != VK_SUCCESS) {
          throw std::runtime_error("failed to create swap chain!");
        }

        swap_chain.reset(temp_swap_chain);
      }
    }

    std::vector<std::unique_ptr<std::remove_pointer_t<VkImageView>, std::function<void(VkImageView)>>>
      swap_chain_image_views;
    {
      // Retrieving the swap chain images

      std::vector<VkImage> swap_chain_images;
      {
        uint32_t image_count;

        vkGetSwapchainImagesKHR(device.get(), swap_chain.get(), &image_count, nullptr);
        swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device.get(), swap_chain.get(), &image_count, swap_chain_images.data());
      }

      // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Image_views

      for (const auto& swap_chain_image : swap_chain_images) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swap_chain_image;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = details.formats[1].format; // should be B8G8R8A8Srgb : SrgbNonlinear
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkImageView temp_image_view;
        if (vkCreateImageView(device.get(), &create_info, nullptr, &temp_image_view) != VK_SUCCESS) {
          throw std::runtime_error("failed to create image views!");
        }

        swap_chain_image_views.emplace_back(temp_image_view, [&device](VkImageView image_view) {
          vkDestroyImageView(device.get(), image_view, nullptr);
        });
      }
    }

    std::unique_ptr<std::remove_pointer_t<VkShaderModule>, std::function<void(VkShaderModule)>>
      vert_shader_module{nullptr, [&device](VkShaderModule shader_module) {
        vkDestroyShaderModule(device.get(), shader_module, nullptr);
      }};
    std::unique_ptr<std::remove_pointer_t<VkShaderModule>, std::function<void(VkShaderModule)>>
      frag_shader_module{nullptr, [&device](VkShaderModule shader_module) {
        vkDestroyShaderModule(device.get(), shader_module, nullptr);
      }};
    VkPipelineShaderStageCreateInfo shader_stages[2]{};
    {
      // https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
      {
        std::ifstream file(executable_dir / "vert.spv", std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
          std::ostringstream oss;
          oss << "failed to open file \"" << (executable_dir / "vert.spv") << '\"';
          throw std::runtime_error(oss.str());
        }
        const size_t file_size = (size_t) file.tellg();
        std::vector<char, AlignedAllocator<char, uint32_t>> code(file_size);
        file.seekg(0);
        file.read(code.data(), static_cast<std::streamsize>(file_size));

        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module;
        if (vkCreateShaderModule(device.get(), &create_info, nullptr, &shader_module) != VK_SUCCESS) {
          throw std::runtime_error("failed to create shader module!");
        }

        vert_shader_module.reset(shader_module);
      }

      {
        std::ifstream file(executable_dir / "frag.spv", std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
          std::ostringstream oss;
          oss << "failed to open file \"" << (executable_dir / "frag.spv") << '\"';
          throw std::runtime_error(oss.str());
        }
        const size_t file_size = (size_t) file.tellg();
        std::vector<char, AlignedAllocator<char, uint32_t>> code(file_size);
        file.seekg(0);
        file.read(code.data(), static_cast<std::streamsize>(file_size));

        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        // TODO it is unclear whether vkCreateShaderModule copies the
        // code or assumes that the vector lives long enough
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module;
        if (vkCreateShaderModule(device.get(), &create_info, nullptr, &shader_module) != VK_SUCCESS) {
          throw std::runtime_error("failed to create shader module!");
        }

        frag_shader_module.reset(shader_module);
      }

      VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
      vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
      vert_shader_stage_info.module = vert_shader_module.get();
      vert_shader_stage_info.pName = "main";

      VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
      frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      frag_shader_stage_info.module = frag_shader_module.get();
      frag_shader_stage_info.pName = "main";

      shader_stages[0] = vert_shader_stage_info;
      shader_stages[1] = frag_shader_stage_info;
    }

    std::unique_ptr<std::remove_pointer_t<VkRenderPass>,
                    std::function<void(VkRenderPass)>> render_pass{
      nullptr,
      [&device](VkRenderPass render_pass) {
        vkDestroyRenderPass(device.get(), render_pass, nullptr);
      }
    };
    std::unique_ptr<std::remove_pointer_t<VkPipelineLayout>,
                    std::function<void(VkPipelineLayout)>> pipeline_layout{
      nullptr,
      [&device](VkPipelineLayout pipeline_layout) {
        vkDestroyPipelineLayout(device.get(), pipeline_layout, nullptr);
      }
    };
    std::unique_ptr<std::remove_pointer_t<VkPipeline>, std::function<void(VkPipeline)>> graphics_pipeline{
      nullptr,
      [&device](VkPipeline graphics_pipeline) {
        vkDestroyPipeline(device.get(), graphics_pipeline, nullptr);
      }
    };
    {
      // https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes

      {
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0; // Optional
        pipeline_layout_info.pSetLayouts = nullptr; // Optional
        pipeline_layout_info.pushConstantRangeCount = 0; // Optional
        pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

        VkPipelineLayout temp_pipeline_layout;
        if (vkCreatePipelineLayout(device.get(), &pipeline_layout_info, nullptr, &temp_pipeline_layout) != VK_SUCCESS) {
          throw std::runtime_error("failed to create pipeline layout!");
        }

        pipeline_layout.reset(temp_pipeline_layout);
      }

      VkAttachmentDescription color_attachment{};
      color_attachment.format = details.formats[1].format;
      color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
      color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      VkAttachmentReference color_attachment_ref{};
      color_attachment_ref.attachment = 0;
      color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass{};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &color_attachment_ref;

      {
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        VkRenderPass temp_render_pass;
        if (vkCreateRenderPass(device.get(), &render_pass_info, nullptr, &temp_render_pass) != VK_SUCCESS) {
          throw std::runtime_error("failed to create render pass!");
        }

        render_pass.reset(temp_render_pass);
      }

      std::vector<VkDynamicState> dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

      VkPipelineDynamicStateCreateInfo dynamic_state{};
      dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
      dynamic_state.pDynamicStates = dynamic_states.data();

      VkPipelineVertexInputStateCreateInfo vertex_input_info{};
      vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
#if 0
      vertex_input_info.vertexBindingDescriptionCount = 0;
      vertex_input_info.pVertexBindingDescriptions = nullptr; // Optional
      vertex_input_info.vertexAttributeDescriptionCount = 0;
      vertex_input_info.pVertexAttributeDescriptions = nullptr; // Optional
#endif

      auto bindingDescription = Vertex::getBindingDescription();
      auto attributeDescriptions = Vertex::getAttributeDescriptions();

      vertex_input_info.vertexBindingDescriptionCount = 1;
      vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
      vertex_input_info.pVertexBindingDescriptions = &bindingDescription;
      vertex_input_info.pVertexAttributeDescriptions = attributeDescriptions.data();

      VkPipelineInputAssemblyStateCreateInfo input_assembly{};
      input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      input_assembly.primitiveRestartEnable = VK_FALSE;

      VkViewport viewport{};
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.width = (float) actual_extent.width;
      viewport.height = (float) actual_extent.height;
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor{};
      scissor.offset = {0, 0};
      scissor.extent = actual_extent;

      VkPipelineViewportStateCreateInfo viewport_state{};
      viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewport_state.viewportCount = 1;
      viewport_state.pViewports = &viewport;
      viewport_state.scissorCount = 1;
      viewport_state.pScissors = &scissor;

      VkPipelineRasterizationStateCreateInfo rasterizer{};
      rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterizer.depthClampEnable = VK_FALSE;
      rasterizer.rasterizerDiscardEnable = VK_FALSE;
      rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
      rasterizer.lineWidth = 1.0f;
      rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
      rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
      rasterizer.depthBiasEnable = VK_FALSE;
      rasterizer.depthBiasConstantFactor = 0.0f; // Optional
      rasterizer.depthBiasClamp = 0.0f; // Optional
      rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

      VkPipelineMultisampleStateCreateInfo multisampling{};
      multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisampling.sampleShadingEnable = VK_FALSE;
      multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      multisampling.minSampleShading = 1.0f; // Optional
      multisampling.pSampleMask = nullptr; // Optional
      multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
      multisampling.alphaToOneEnable = VK_FALSE; // Optional

      VkPipelineColorBlendAttachmentState color_blend_attachment{};
      color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      color_blend_attachment.blendEnable = VK_FALSE;
      color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
      color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
      color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
      color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
      color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
      color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
      color_blend_attachment.blendEnable = VK_TRUE;
      color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

      VkPipelineColorBlendStateCreateInfo color_blending{};
      color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      color_blending.logicOpEnable = VK_FALSE;
      color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
      color_blending.attachmentCount = 1;
      color_blending.pAttachments = &color_blend_attachment;
      color_blending.blendConstants[0] = 0.0f; // Optional
      color_blending.blendConstants[1] = 0.0f; // Optional
      color_blending.blendConstants[2] = 0.0f; // Optional
      color_blending.blendConstants[3] = 0.0f; // Optional

      {
        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = sizeof(shader_stages) / sizeof(VkPipelineShaderStageCreateInfo);
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = nullptr; // Optional
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = pipeline_layout.get();
        pipeline_info.renderPass = render_pass.get();
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipeline_info.basePipelineIndex = -1; // Optional

        VkPipeline temp_graphics_pipeline;
        if (vkCreateGraphicsPipelines(device.get(),
                                      VK_NULL_HANDLE,
                                      1,
                                      &pipeline_info,
                                      nullptr,
                                      &temp_graphics_pipeline) != VK_SUCCESS) {
          throw std::runtime_error("failed to create graphics pipeline!");
        }

        graphics_pipeline.reset(temp_graphics_pipeline);
      }
    }

    std::vector<std::unique_ptr<std::remove_pointer_t<VkFramebuffer>, std::function<void(VkFramebuffer)>>>
      swap_chain_framebuffers;
    {
      // https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Framebuffers

      for (const auto& swap_chain_image_view : swap_chain_image_views) {
        VkImageView attachments[] = {
          swap_chain_image_view.get()
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass.get();
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = actual_extent.width;
        framebuffer_info.height = actual_extent.height;
        framebuffer_info.layers = 1;

        VkFramebuffer swap_chain_framebuffer;
        if (vkCreateFramebuffer(device.get(), &framebuffer_info, nullptr, &swap_chain_framebuffer) != VK_SUCCESS) {
          throw std::runtime_error("failed to create framebuffer!");
        }

        swap_chain_framebuffers.emplace_back(swap_chain_framebuffer, [&device](VkFramebuffer framebuffer) {
          vkDestroyFramebuffer(device.get(), framebuffer, nullptr);
        });
      }
    }

    std::unique_ptr<std::remove_pointer_t<VkCommandPool>, std::function<void(VkCommandPool)>> command_pool{
      nullptr,
      [&device](VkCommandPool command_pool) {
        vkDestroyCommandPool(device.get(), command_pool, nullptr);
      }
    };
    std::unique_ptr<std::remove_pointer_t<VkCommandBuffer>, std::function<void(VkCommandBuffer)>> command_buffer{
      nullptr,
      [&device, &command_pool](VkCommandBuffer command_buffer) {
        vkFreeCommandBuffers(device.get(), command_pool.get(), 1, &command_buffer);
      }
    };
    {
      // https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers

      {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family_index.value();
        VkCommandPool temp_command_pool;
        if (vkCreateCommandPool(device.get(), &pool_info, nullptr, &temp_command_pool) != VK_SUCCESS) {
          throw std::runtime_error("failed to create command pool!");
        }

        command_pool.reset(temp_command_pool);
      }

      VkCommandBufferAllocateInfo alloc_info{};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.commandPool = command_pool.get();
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandBufferCount = 1;

      VkCommandBuffer temp_command_buffer;
      if (vkAllocateCommandBuffers(device.get(), &alloc_info, &temp_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
      }

      command_buffer.reset(temp_command_buffer);
    }

    // https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation

    std::unique_ptr<std::remove_pointer_t<VkSemaphore>, std::function<void(VkSemaphore)>> image_available_semaphore{
      nullptr,
      [&device](VkSemaphore semaphore) {
        vkDestroySemaphore(device.get(), semaphore, nullptr);
      }
    };
    std::unique_ptr<std::remove_pointer_t<VkSemaphore>, std::function<void(VkSemaphore)>> render_finished_semaphore{
      nullptr,
      [&device](VkSemaphore semaphore) {
        vkDestroySemaphore(device.get(), semaphore, nullptr);
      }
    };
    std::unique_ptr<std::remove_pointer_t<VkFence>, std::function<void(VkFence)>> in_flight_fence{
      nullptr,
      [&device](VkFence fence) {
        vkDestroyFence(device.get(), fence, nullptr);
      }
    };

    {
      VkSemaphore temp_image_available_semaphore;
      VkSemaphore temp_render_finished_semaphore;
      VkFence temp_in_flight_fence;

      VkSemaphoreCreateInfo semaphoreInfo{};
      semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

      VkFenceCreateInfo fenceInfo{};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

      if (vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &temp_image_available_semaphore) != VK_SUCCESS ||
          vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &temp_render_finished_semaphore) != VK_SUCCESS ||
          vkCreateFence(device.get(), &fenceInfo, nullptr, &temp_in_flight_fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create semaphores!");
      }

      image_available_semaphore.reset(temp_image_available_semaphore);
      render_finished_semaphore.reset(temp_render_finished_semaphore);
      in_flight_fence.reset(temp_in_flight_fence);
    }

    // vertex buffers: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(vertices[0]) * vertices.size();
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    std::unique_ptr<std::remove_pointer_t<VkBuffer>, std::function<void(VkBuffer)>> vertex_buffer{
      nullptr,
      [&device](VkBuffer buffer) {
        vkDestroyBuffer(device.get(), buffer, nullptr);
      }
    };

    {
      VkBuffer temp_buffer;
      if (vkCreateBuffer(device.get(), &buffer_info, nullptr, &temp_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
      }

      vertex_buffer.reset(temp_buffer);
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device.get(), vertex_buffer.get(), &mem_requirements);
    std::cout << "memory requirements size: " << mem_requirements.size << '\n';

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_devices[0], &mem_properties);

    const uint32_t type_filter = mem_requirements.memoryTypeBits;
    const VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    std::unique_ptr<std::remove_pointer_t<VkDeviceMemory>, std::function<void(VkDeviceMemory)>> vertex_buffer_memory{
      nullptr,
      [&device](VkDeviceMemory mem) {
        vkFreeMemory(device.get(), mem, nullptr);
      }
    };
    {
      assert(mem_properties.memoryTypeCount < sizeof(type_filter) * 8);
      uint32_t index = 0U;
      const auto suitable_memory_type =
        std::find_if(&mem_properties.memoryTypes[0], &mem_properties.memoryTypes[mem_properties.memoryTypeCount],
                     [&index, &properties, &type_filter](const auto& element) {
                       return ((type_filter & (1 << index++)) && (element.propertyFlags & properties) == properties);
                     });

      if (suitable_memory_type == &mem_properties.memoryTypes[mem_properties.memoryTypeCount]) {
        throw std::runtime_error("no suitable memory found");
      }

      VkMemoryAllocateInfo alloc_info{};
      alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc_info.allocationSize = mem_requirements.size;
      alloc_info.memoryTypeIndex = index - 1;

      VkDeviceMemory temp_vertex_buffer_memory;
      if (vkAllocateMemory(device.get(), &alloc_info, nullptr, &temp_vertex_buffer_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
      }

      vertex_buffer_memory.reset(temp_vertex_buffer_memory);
    }

    vkBindBufferMemory(device.get(), vertex_buffer.get(), vertex_buffer_memory.get(), 0);

    void* data;
    vkMapMemory(device.get(), vertex_buffer_memory.get(), 0, buffer_info.size, 0, &data);
    memcpy(data, vertices.data(), (size_t) buffer_info.size);
    vkUnmapMemory(device.get(), vertex_buffer_memory.get());

    window.show();
    while (!window.should_close()) {
      context.clear();

      VkFence fences[] = {in_flight_fence.get()};
      vkWaitForFences(device.get(), 1, fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
      vkResetFences(device.get(), 1, fences);

      uint32_t image_index;
      vkAcquireNextImageKHR(device.get(),
                            swap_chain.get(),
                            std::numeric_limits<uint64_t>::max(),
                            image_available_semaphore.get(),
                            VK_NULL_HANDLE,
                            &image_index);

      vkResetCommandBuffer(command_buffer.get(), 0);

      record_command_buffer(*command_buffer, *graphics_pipeline, *render_pass,
                            *swap_chain_framebuffers[image_index], actual_extent, vertex_buffer.get());

      // record command buffer
      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

      VkSemaphore waitSemaphores[] = {image_available_semaphore.get()};
      VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = waitSemaphores;
      submitInfo.pWaitDstStageMask = waitStages;
      submitInfo.commandBufferCount = 1;
      VkCommandBuffer command_buffers[] = {command_buffer.get()};
      submitInfo.pCommandBuffers = command_buffers;

      VkSemaphore signalSemaphores[] = {render_finished_semaphore.get()};
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = signalSemaphores;

      if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fence.get()) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
      }

      VkPresentInfoKHR presentInfo{};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = signalSemaphores;
      VkSwapchainKHR swapChains[] = {swap_chain.get()};
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = swapChains;
      presentInfo.pImageIndices = &image_index;
      presentInfo.pResults = nullptr; // Optional

      vkQueuePresentKHR(graphics_queue, &presentInfo);

      context.pool_events();
    }

    vkDeviceWaitIdle(device.get());
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
