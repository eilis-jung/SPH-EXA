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

set(CSTONE_DIR ${CMAKE_SOURCE_DIR}/domain/include)

# Vulkan dependencies -- only tested on Ubuntu 22.04!
set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/extern)
if(UNIX)
    if(NOT APPLE)
        set(vulkan_include ${THIRD_PARTY_DIR}/VulkanSDK/include)
        set(vulkan_lib ${THIRD_PARTY_DIR}/VulkanSDK/lib/libvulkan.so.1)
        set(glslangValidator_executable ${THIRD_PARTY_DIR}/VulkanSDK/bin/glslangValidator)
        add_compile_definitions("VK_LAYER_PATH=${THIRD_PARTY_DIR}/VulkanSDK/bin")
        set(VULKAN_LIB "${THIRD_PARTY_DIR}/VulkanSDK/Lib")
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${VULKAN_LIB})
    endif()
else()
    message(FATAL_ERROR "Unknown Platform!")
endif()

# Special compiler flags for Vulkan
# Wunused-result is a usual practice in Vulkan programming -- has to be ignored
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Werror -Weverything)
    add_compile_options(-Wno-unused-result)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	add_compile_options(-O2)
	add_compile_options(-Wno-unused-result)
endif()


add_subdirectory ("extern")
# Shader compiler
set(SHADER_COMPILE_TARGET ShaderCompile)
add_subdirectory(shader)
add_subdirectory(src)

# if (BUILD_TESTING)
#   add_subdirectory(test)
# endif ()