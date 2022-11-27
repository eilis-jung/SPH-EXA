cmake_minimum_required(VERSION 3.17)

project(visualization CXX)

set(CMAKE_CXX_STANDARD 20)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    find_package(CUDAToolkit)
    set(CMAKE_CUDA_STANDARD 17)
endif()

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Vulkan dependencies -- only tested on Ubuntu 22.04!
set(VISUALIZATION_EXTERN_PATH "${PROJECT_SOURCE_DIR}/extern")
if(UNIX)
    if(NOT APPLE)
        set(vulkan_include ${VISUALIZATION_EXTERN_PATH}/VulkanSDK/include)
        set(vulkan_lib ${VISUALIZATION_EXTERN_PATH}/VulkanSDK/lib/libvulkan.so.1)
        set(glslangValidator_executable ${VISUALIZATION_EXTERN_PATH}/VulkanSDK/bin/glslangValidator)
        add_compile_definitions("VK_LAYER_PATH=${VISUALIZATION_EXTERN_PATH}/VulkanSDK/bin")
        set(VULKAN_LIB "${VISUALIZATION_EXTERN_PATH}/VulkanSDK/Lib")
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${VULKAN_LIB})
    endif()
else()
    message(FATAL_ERROR "Unknown Platform!")
endif()

# Shader compiler
set(SHADER_COMPILE_TARGET ShaderCompile)
add_subdirectory(shader)

set(CSTONE_DIR ${CMAKE_SOURCE_DIR}/domain/include)

# Install assets
add_subdirectory ("extern")
add_subdirectory(src)

# if (BUILD_TESTING)
#   add_subdirectory(test)
# endif ()