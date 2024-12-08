cmake_minimum_required(VERSION 3.29)

project(vulkan_glfw LANGUAGES CXX)

# in GLFW < 3.3, the gamepad wouldn't work
find_package(Catch2 REQUIRED)
find_package(cxxopts REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)

include(CheckPIESupported)
check_pie_supported(LANGUAGES CXX)

if (NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
  message(WARNING "PIE is not supported at link time")
endif()

macro(add_shader target type input output)
  if (NOT ((${type} STREQUAL "vertex") OR (${type} STREQUAL "frag")))
    message(FATAL_ERROR "unknown shader type \"${type}\"")
  endif()

  add_custom_command(
    OUTPUT
    ${output}
    COMMAND
    Vulkan::glslc -fshader-stage=${type} -o ${output} ${input} $<$<CONFIG:Release>:-O> $<$<CONFIG:Debug>:-g>
    DEPENDS
    ${input}
    COMMENT "compiling ${input}")

  add_custom_target(${target} DEPENDS ${output})
endmacro()

add_library(graphics STATIC graphics.cpp)
target_link_libraries(graphics PUBLIC glfw Vulkan::Vulkan)

add_executable(sample
  executable_info.cpp
  main.cpp)
target_compile_options(sample PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wconversion;-Wno-unused-parameter>
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-fsanitize=address>
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:Clang,GNU>>:-fsanitize=undefined>
)
target_link_options(sample PRIVATE
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-fsanitize=address>
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:Clang,GNU>>:-fsanitize=undefined>
)
add_shader(vert_spirv vertex ${CMAKE_SOURCE_DIR}/vert.glsl $<CONFIG>/vert.spv)
add_shader(frag_spirv frag ${CMAKE_SOURCE_DIR}/frag.glsl $<CONFIG>/frag.spv)
add_dependencies(sample vert_spirv)
add_dependencies(sample frag_spirv)
target_compile_features(sample PRIVATE cxx_std_17)
set_property(TARGET sample PROPERTY POSITION_INDEPENDENT_CODE ON)
target_precompile_headers(sample
  PRIVATE
  allocator.hpp
  executable_info.hpp
  [["cxxopts.hpp"]])
target_link_libraries(sample
  PRIVATE
  cxxopts::cxxopts
  glfw
  glm::glm
  graphics
  Vulkan::Vulkan
)

add_executable(joy
  joy.cpp)
target_compile_options(joy PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wconversion;-Wno-unused-parameter>)
target_compile_features(joy PRIVATE cxx_std_17)
target_link_libraries(joy
  PRIVATE
  glfw
  graphics
)

add_executable(monitor
  monitor.cpp)
target_compile_options(monitor PRIVATE
  $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wconversion;-Wno-unused-parameter>)
target_compile_features(monitor PRIVATE cxx_std_17)
target_link_libraries(monitor
  PRIVATE
  glfw
  graphics
)

add_executable(test_allocator test_allocator.cpp)
target_compile_features(test_allocator PRIVATE cxx_std_17)
target_link_libraries(test_allocator PRIVATE Catch2::Catch2WithMain)
