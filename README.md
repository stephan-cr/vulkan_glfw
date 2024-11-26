# Vulkan example

My sources for the Vulkan triangle tutorial. It deviates from the
original tutorial at certain points:

- use `std::unqiue_ptr` to ensure that memory is released
  automatically
- don't use the class structure given in the tutorial
- create wrapper classes for GLFW
- use CMake for compilation

## Resources

- currently used guide: https://vulkan-tutorial.com/
- another guide: https://vkguide.dev/
