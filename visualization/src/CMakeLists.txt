set(VISUALIZATION_EXTERN_PATH "${PROJECT_SOURCE_DIR}/extern")
set(GENERATED_SHADERS_PATH "${PROJECT_SOURCE_DIR}/shader/generated")
include_directories("${VISUALIZATION_EXTERN_PATH}/glm")
include_directories("${VISUALIZATION_EXTERN_PATH}/VulkanSDK/include")
include_directories("${VISUALIZATION_EXTERN_PATH}/glfw/include")
include_directories("${VISUALIZATION_EXTERN_PATH}/stb")

include_directories("${GENERATED_SHADERS_PATH}")
# add_library(visualization core.cpp)

# Build 3rd-party tools
file(GLOB GLFW_LIBRARY ${VISUALIZATION_EXTERN_PATH}/glfw/glfw3.lib)


file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")
message(${Vulkan_LIBRARY})
add_executable(visualization ${SOURCES} ${HEADERS})
add_dependencies(visualization glfw ${SHADER_COMPILE_TARGET})
target_link_libraries(visualization glfw ${Vulkan_LIBRARY})
