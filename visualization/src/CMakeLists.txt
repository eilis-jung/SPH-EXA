set(GENERATED_SHADERS_PATH "${PROJECT_SOURCE_DIR}/shader/generated")

# Special compiler flags for Vulkan
# Wunused-result is a usual practice in Vulkan programming -- has to be ignored
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Werror -Weverything)
    add_compile_options(-Wno-unused-result)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# add_compile_options(-O2)
	add_compile_options(-Wno-unused-result)
endif()

set(CURR_TARGET_NAME visualization)

# Build 3rd-party tools
file(GLOB GLFW_LIBRARY ${VISUALIZATION_EXTERN_PATH}/glfw/glfw3.lib)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")



# add_library(${CURR_TARGET_NAME} STATIC ${SOURCES} ${HEADERS})
add_executable(${CURR_TARGET_NAME} ${SOURCES} ${HEADERS})
add_dependencies(${CURR_TARGET_NAME} glfw ${SHADER_COMPILE_TARGET})
target_link_libraries(${CURR_TARGET_NAME} glfw ${Vulkan_LIBRARY})

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${CURR_TARGET_NAME}  OpenMP::OpenMP_CXX)
endif()

# Include 3rd-party headers
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${VISUALIZATION_EXTERN_PATH}/glm")
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${VISUALIZATION_EXTERN_PATH}/VulkanSDK/include")
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${VISUALIZATION_EXTERN_PATH}/glfw/include")
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${VISUALIZATION_EXTERN_PATH}/stb")
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${VISUALIZATION_EXTERN_PATH}/tinyobjloader")
target_include_directories(${CURR_TARGET_NAME} PUBLIC "${GENERATED_SHADERS_PATH}")