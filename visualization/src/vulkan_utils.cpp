#include "vulkan_utils.h"

using namespace sphexa;

vk::VertexInputBindingDescription VulkanUtils::getBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;
    return bindingDescription;
}

std::vector<vk::VertexInputAttributeDescription> VulkanUtils::getAttributeDescriptions()
{
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.resize(3);

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset   = offsetof(Vertex, position);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset   = offsetof(Vertex, color);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = vk::Format::eR32G32Sfloat;
    attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

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