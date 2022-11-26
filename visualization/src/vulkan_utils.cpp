#include "vulkan_utils.h"

using namespace sphexa;

vk::VertexInputBindingDescription VulkanUtils::getBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription {};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;
    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 5> VulkanUtils::getAttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 5> attributeDescriptions {};

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset   = offsetof(Vertex, position);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset   = offsetof(Vertex, velocity);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset   = offsetof(Vertex, attr1);

    attributeDescriptions[3].binding  = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[3].offset   = offsetof(Vertex, attr2);

    attributeDescriptions[4].binding  = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[4].offset   = offsetof(Vertex, color);

    return attributeDescriptions;
}

vk::UniqueShaderModule VulkanUtils::createShaderModule(vk::UniqueDevice&                 device,
                                                               const std::vector<unsigned char>& shader_code)

{
    try
    {
        return device->createShaderModuleUnique(
            {vk::ShaderModuleCreateFlags(), shader_code.size(), reinterpret_cast<const uint32_t*>(shader_code.data())});
    }
    catch (vk::SystemError err)
    {
        throw std::runtime_error("failed to create shader module!");
    }
}