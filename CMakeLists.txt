cmake_minimum_required(VERSION 3.24)

project(vulkan_sample LANGUAGES CXX)

# in GLFW < 3.3, the gamepad wouldn't work
find_package(glfw3 3.3 REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)

macro(add_shader target type input output)
  if (NOT ((${type} STREQUAL "vertex") OR (${type} STREQUAL "frag")))
    message(FATAL_ERROR "unknown shader type \"${type}\"")
  endif()

  add_custom_command(
    OUTPUT
    ${output}
    COMMAND
    Vulkan::glslc -fshader-stage=${type} -o vert.spv ${input}
    DEPENDS
    ${input}
    COMMENT "compiling ${input}")

  add_custom_target(${target} DEPENDS ${output})
endmacro()

add_executable(sample
  main.cpp)
target_compile_options(sample PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wno-unused-parameter>)
add_shader(vert_spirv vertex ${CMAKE_SOURCE_DIR}/vert.glsl vert.spv)
add_dependencies(sample vert_spirv)
target_compile_features(sample PRIVATE cxx_std_17)
target_link_libraries(sample
  PRIVATE
  glfw
  Vulkan::Vulkan
)

add_executable(joy
  joy.cpp)
target_compile_options(joy PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wno-unused-parameter>)
target_compile_features(joy PRIVATE cxx_std_17)
target_link_libraries(joy
  PRIVATE
  glfw
)
