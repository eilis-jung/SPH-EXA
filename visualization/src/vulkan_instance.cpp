#include "vulkan_instance.h"
#include "window.h"
#include <stb_image.h>

namespace sphexa
{
    void
    VulkanInstance::init(GLFWwindow* pWindow, std::shared_ptr<Elements> pElements)
    {
        m_window        = pWindow;
        m_elements     = std::shared_ptr<Elements>(pElements);
        m_cellVertArray = new int[m_elements->m_numGridCells * 6] {0};
        m_cellVertCount = new int[m_elements->m_numGridCells] {0};
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffers();
        createIndexBuffer();
        createNumVertsBuffer();
        createCellVertArrayBuffer();
        createCellVertCountBuffer();
        createSphereVertsBuffer();
        createComputePipeline(PHYSICSCOMPUTE_COMP, m_computePipelinePhysics);
        createComputePipeline(FILLCELLVERTEXINFO_COMP, m_computePipelineFillCellVertex);
        createComputePipeline(RESETCELLVERTEXINFO_COMP, m_computePipelineResetCellVertex);
        createComputePipeline(SPHEREVERTEXCOMPUTE_COMP, m_computePipelineSphereVertex);
        createuniformUboBuffers();
        // Descriptor sets can't be created directly, they must be allocated from a pool like command buffers.
        // Allocate one of these descriptors for every frame.
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }
    void VulkanInstance::cleanup()
    {
        cleanupSwapChain();
        m_device->destroyPipeline(m_computePipelinePhysics);
        m_device->destroyPipeline(m_computePipelineFillCellVertex);
        m_device->destroyPipeline(m_computePipelineResetCellVertex);
        m_device->destroyPipeline(m_computePipelineSphereVertex);
        m_device->destroyPipelineLayout(m_computePipelineLayout);
        m_device->destroyDescriptorPool(m_computeDescriptorPool);
        m_device->destroyDescriptorSetLayout(m_computeDescriptorSetLayout);

        m_device->destroyPipeline(m_graphicsPipeline);
        m_device->destroyPipelineLayout(m_pipelineLayout);
        m_device->destroyRenderPass(m_renderPass);
        m_device->destroyDescriptorPool(m_descriptorPool);

        // The main texture image is used until the end of the program:
        m_device->destroySampler(m_textureSampler);
        m_device->destroyImageView(m_textureImageView);
        m_device->destroyImage(m_textureImage);
        m_device->freeMemory(m_textureImageMemory);
        m_device->destroyBuffer(m_numVertsBuffer);
        m_device->freeMemory(m_numVertsBufferMemory);
        m_device->destroyBuffer(m_vertexBuffer1);
        m_device->freeMemory(m_vertexBufferMemory1);
        m_device->destroyBuffer(m_vertexBuffer2);
        m_device->freeMemory(m_vertexBufferMemory2);
        m_device->destroyBuffer(m_indexBuffer);
        m_device->freeMemory(m_indexBufferMemory);
        m_device->destroyBuffer(m_cellVertArrayBuffer);
        m_device->freeMemory(m_cellVertArrayBufferMemory);
        m_device->destroyBuffer(m_cellVertCountBuffer);
        m_device->freeMemory(m_cellVertCountBufferMemory);
        m_device->destroyBuffer(m_sphereVertsBuffer);
        m_device->freeMemory(m_sphereVertsBufferMemory);

        for (size_t i = 0; i < m_max_frames_in_flight; i++)
        {
            m_device->destroySemaphore(m_renderFinishedSemaphores[i]);
            m_device->destroySemaphore(m_imageAvailableSemaphores[i]);
            m_device->destroyFence(m_inFlightFences[i]);
        }
        m_device->destroyCommandPool(m_commandPool);
        m_instance->destroySurfaceKHR(m_surface);

        /*if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(*instance, callback, nullptr);
        }*/
    }
    void VulkanInstance::drawFrame()
    {
        m_device->waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        uint32_t imageIndex;
        try
        {
            vk::ResultValue result = m_device->acquireNextImageKHR(
                m_swapChain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[m_currentFrame], nullptr);
            imageIndex = result.value;
        }
        catch (vk::OutOfDateKHRError err)
        {
            recreateSwapChain();
            return;
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vk::DeviceSize bufferSize = static_cast<uint32_t>(m_elements->m_raw_verts.size() * sizeof(Vertex));
        copyBuffer(m_vertexBuffer2, m_vertexBuffer1, bufferSize);
        updateUniformBuffer(imageIndex);
        vk::SubmitInfo submitInfo = {};

        vk::Semaphore          waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
        vk::PipelineStageFlags waitStages[]     = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        submitInfo.waitSemaphoreCount           = 1;
        submitInfo.pWaitSemaphores              = waitSemaphores;
        submitInfo.pWaitDstStageMask            = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &m_commandBuffers[imageIndex];

        vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
        submitInfo.signalSemaphoreCount  = 1;
        submitInfo.pSignalSemaphores     = signalSemaphores;

        m_device->resetFences(1, &m_inFlightFences[m_currentFrame]);

        try
        {
            m_graphicsQueue.submit(submitInfo, m_inFlightFences[m_currentFrame]);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::PresentInfoKHR presentInfo = {};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSemaphores;

        vk::SwapchainKHR swapChains[] = {m_swapChain};
        presentInfo.swapchainCount    = 1;
        presentInfo.pSwapchains       = swapChains;
        presentInfo.pImageIndices     = &imageIndex;

        vk::Result resultPresent;
        try
        {
            resultPresent = m_presentQueue.presentKHR(presentInfo);
        }
        catch (vk::OutOfDateKHRError err)
        {
            resultPresent = vk::Result::eErrorOutOfDateKHR;
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        if (resultPresent == vk::Result::eSuboptimalKHR || resultPresent == vk::Result::eSuboptimalKHR ||
            m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapChain();
            return;
        }

        m_currentFrame = (m_currentFrame + 1) % m_max_frames_in_flight;
    }

    void VulkanInstance::idle() { m_device->waitIdle(); }

    bool VulkanInstance::checkValidationLayerSupport()
    {
        auto availableLayers = vk::enumerateInstanceLayerProperties();
        for (const char* layerName : VulkanUtils::validationLayers)
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }
    std::vector<const char*> VulkanInstance::getRequiredExtensions()
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }
    void VulkanInstance::createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        auto appInfo = vk::ApplicationInfo(
            "Title", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0);

        auto extensions = getRequiredExtensions();

        auto createInfo = vk::InstanceCreateInfo(vk::InstanceCreateFlags(),
                                                 &appInfo,
                                                 0,
                                                 nullptr,
                                                 static_cast<uint32_t>(extensions.size()),
                                                 extensions.data());

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(VulkanUtils::validationLayers.size());
            createInfo.ppEnabledLayerNames = VulkanUtils::validationLayers.data();
        }

        try
        {
            m_instance = vk::createInstanceUnique(createInfo, nullptr);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }
    void VulkanInstance::createSurface()
    {
        VkSurfaceKHR rawSurface;
        if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }

        m_surface = rawSurface;
    }
    bool VulkanInstance::isDeviceSuitable(const vk::PhysicalDevice& device)
    {
        VulkanUtils::QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            VulkanUtils::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        vk::PhysicalDeviceFeatures supportedFeatures;
        device.getFeatures(&supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    VulkanUtils::QueueFamilyIndices VulkanInstance::findQueueFamilies(vk::PhysicalDevice device)
    {
        VulkanUtils::QueueFamilyIndices indices;

        auto queueFamilies = device.getQueueFamilyProperties();

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
            }

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eCompute)
            {
                indices.computeFamily = i;
            }

            if (queueFamily.queueCount > 0 && device.getSurfaceSupportKHR(i, m_surface))
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    bool VulkanInstance::checkDeviceExtensionSupport(const vk::PhysicalDevice& device)
    {
        std::set<std::string> requiredExtensions(VulkanUtils::deviceExtensions.begin(),
                                                 VulkanUtils::deviceExtensions.end());

        for (const auto& extension : device.enumerateDeviceExtensionProperties())
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    VulkanUtils::SwapChainSupportDetails VulkanInstance::querySwapChainSupport(const vk::PhysicalDevice& device)
    {
        VulkanUtils::SwapChainSupportDetails details;
        details.capabilities = device.getSurfaceCapabilitiesKHR(m_surface);
        details.formats      = device.getSurfaceFormatsKHR(m_surface);
        details.presentModes = device.getSurfacePresentModesKHR(m_surface);

        return details;
    }

    void VulkanInstance::pickPhysicalDevice()
    {
        auto devices = m_instance->enumeratePhysicalDevices();
        if (devices.size() == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        for (const auto& device : devices)
        {
            if (isDeviceSuitable(device))
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (!m_physicalDevice)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }
    void VulkanInstance::createLogicalDevice()
    {
        VulkanUtils::QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t>                     uniqueQueueFamilies = {
            indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value()};

        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            queueCreateInfos.push_back({vk::DeviceQueueCreateFlags(),
                                        queueFamily,
                                        1, // queueCount
                                        &queuePriority});
        }

        auto deviceFeatures              = vk::PhysicalDeviceFeatures();
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        auto createInfo                  = vk::DeviceCreateInfo(
            vk::DeviceCreateFlags(), static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data());
        createInfo.pEnabledFeatures        = &deviceFeatures;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(VulkanUtils::deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = VulkanUtils::deviceExtensions.data();

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(VulkanUtils::validationLayers.size());
            createInfo.ppEnabledLayerNames = VulkanUtils::validationLayers.data();
        }

        try
        {
            m_device = m_physicalDevice.createDeviceUnique(createInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        m_graphicsQueue = m_device->getQueue(indices.graphicsFamily.value(), 0);
        m_presentQueue  = m_device->getQueue(indices.presentFamily.value(), 0);
        m_computeQueue  = m_device->getQueue(indices.computeFamily.value(), 0);
    }
    vk::SurfaceFormatKHR
    VulkanInstance::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
        {
            return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
        }

        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }
    vk::PresentModeKHR
    VulkanInstance::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes)
    {
        vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox)
            {
                return availablePresentMode;
            }
            else if (availablePresentMode == vk::PresentModeKHR::eImmediate)
            {
                bestMode = availablePresentMode;
            }
        }

        return bestMode;
    }
    vk::Extent2D VulkanInstance::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_window, &width, &height);

            vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            actualExtent.width  = std::max(capabilities.minImageExtent.width,
                                          std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height,
                                           std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }
    void VulkanInstance::createSwapChain()
    {
        VulkanUtils::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo(vk::SwapchainCreateFlagsKHR(),
                                              m_surface,
                                              imageCount,
                                              surfaceFormat.format,
                                              surfaceFormat.colorSpace,
                                              extent,
                                              1, // imageArrayLayers
                                              vk::ImageUsageFlagBits::eColorAttachment);

        VulkanUtils::QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        uint32_t queueFamilyIndices[]           = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }

        createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode    = presentMode;
        createInfo.clipped        = VK_TRUE;

        createInfo.oldSwapchain = vk::SwapchainKHR(nullptr);

        try
        {
            m_swapChain = m_device->createSwapchainKHR(createInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        m_swapChainImages = m_device->getSwapchainImagesKHR(m_swapChain);

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent      = extent;
    }

    vk::ImageView VulkanInstance::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)

    {
        vk::ImageViewCreateInfo viewInfo {};
        viewInfo.image                           = image;
        viewInfo.viewType                        = vk::ImageViewType::e2D;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkImageView imageView;
        try
        {
            imageView = m_device->createImageView(viewInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void VulkanInstance::createImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            try
            {
                m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat);
            }
            catch (vk::SystemError err)
            {
                std::cerr << "failed to create image views!" << std::endl;
            }
        }
    }

    vk::Format VulkanInstance::findSupportedFormat(const std::vector<vk::Format>& candidates,
                                                   vk::ImageTiling                tiling,
                                                   vk::FormatFeatureFlags         features)

    {
        for (vk::Format format : candidates)
        {
            vk::FormatProperties props;
            m_physicalDevice.getFormatProperties(format, &props);

            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    vk::Format VulkanInstance::findDepthFormat()
    {
        vk::Format depthFormat;
        try
        {
            depthFormat =
                findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                                    vk::ImageTiling::eOptimal,
                                    vk::FormatFeatureFlags(vk::FormatFeatureFlagBits::eDepthStencilAttachment));
        }
        catch (std::runtime_error err)
        {
            std::cerr << err.what() << std::endl;
            depthFormat = vk::Format::eD32Sfloat;
        }

        return depthFormat;
    }

    void VulkanInstance::createRenderPass()
    {
        vk::AttachmentDescription colorAttachment = {};
        colorAttachment.format                    = m_swapChainImageFormat;
        colorAttachment.samples                   = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp                    = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp                   = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp             = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp            = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout             = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout               = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment              = 0;
        colorAttachmentRef.layout                  = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentDescription depthAttachment {};
        depthAttachment.format         = findDepthFormat();
        depthAttachment.samples        = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp         = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp        = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout  = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::AttachmentReference depthAttachmentRef {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::SubpassDescription subpass  = {};
        subpass.pipelineBindPoint       = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef; // Unlike color attachments, a subpass can only use a
                                                               // single depth (+stencil) attachment.

        vk::SubpassDependency dependency = {};
        dependency.srcSubpass            = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass            = 0;
        dependency.srcStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
        // dependency.srcAccessMask = 0;
        dependency.dstStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.dstAccessMask =
            vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eColorAttachmentWrite;

        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.attachmentCount          = static_cast<uint32_t>(attachments.size());
        ;
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        try
        {
            m_renderPass = m_device->createRenderPass(renderPassInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void VulkanInstance::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding {};
        // Specify the binding used in the shader
        uboLayoutBinding.binding = 0;
        // Specify the type of descriptor, which is a uniform buffer object
        uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        // Our MVP transformation is in a single uniform buffer object
        uboLayoutBinding.descriptorCount    = 1;
        uboLayoutBinding.stageFlags         = vk::ShaderStageFlagBits::eVertex;
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional, used for image sampling related descriptors

        vk::DescriptorSetLayoutBinding samplerLayoutBinding {};
        samplerLayoutBinding.binding            = 1;
        samplerLayoutBinding.descriptorCount    = 1;
        samplerLayoutBinding.descriptorType     = vk::DescriptorType::eCombinedImageSampler;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        // Indicate that we intend to use the combined image sampler descriptor in the fragment shader.
        samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo             descriptorLayoutInfo {};
        descriptorLayoutInfo.flags        = vk::DescriptorSetLayoutCreateFlags();
        descriptorLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptorLayoutInfo.pBindings    = bindings.data();

        try
        {
            m_descriptorSetLayout = m_device->createDescriptorSetLayout(descriptorLayoutInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void VulkanInstance::createGraphicsPipeline()
    {
        vk::UniqueShaderModule vertShaderModule = VulkanUtils::createShaderModule(m_device, SHADER_VERT);
        vk::UniqueShaderModule fragShaderModule = VulkanUtils::createShaderModule(m_device, SHADER_FRAG);

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"},
            {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main"}};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.vertexBindingDescriptionCount          = 0;
        vertexInputInfo.vertexAttributeDescriptionCount        = 0;

        auto bindingDescription    = VulkanUtils::getBindingDescription();
        auto attributeDescriptions = VulkanUtils::getAttributeDescriptions();

        vertexInputInfo.vertexBindingDescriptionCount   = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.topology                                 = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable                   = VK_FALSE;

        vk::Viewport viewport = {};
        viewport.x            = 0.0f;
        viewport.y            = 0.0f;
        viewport.width        = (float)m_swapChainExtent.width;
        viewport.height       = (float)m_swapChainExtent.height;
        viewport.minDepth     = 0.0f;
        viewport.maxDepth     = 1.0f;

        vk::Rect2D scissor = {};
        scissor.offset     = vk::Offset2D {0, 0};
        scissor.extent     = m_swapChainExtent;

        vk::PipelineViewportStateCreateInfo viewportState = {};
        viewportState.viewportCount                       = 1;
        viewportState.pViewports                          = &viewport;
        viewportState.scissorCount                        = 1;
        viewportState.pScissors                           = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.depthClampEnable                         = VK_FALSE;
        rasterizer.rasterizerDiscardEnable                  = VK_FALSE;
        rasterizer.polygonMode                              = vk::PolygonMode::eFill;
        rasterizer.lineWidth                                = 1.0f;
        rasterizer.cullMode                                 = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace                                = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable                          = VK_FALSE;

        vk::PipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sampleShadingEnable                    = VK_FALSE;
        multisampling.rasterizationSamples                   = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil {};
        // The depthTestEnable field specifies if the depth of new fragments should be compared to the depth buffer
        // to see if they should be discarded.
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        // Specify the comparison that is performed to keep or discard fragments.
        depthStencil.depthCompareOp        = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds        = 0.0f; // Optional
        depthStencil.maxDepthBounds        = 1.0f; // Optional
        depthStencil.stencilTestEnable     = VK_FALSE;
        // depthStencil.front = {}; // Optional
        // depthStencil.back = {}; // Optional

        vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;

        vk::PipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.logicOpEnable                         = VK_FALSE;
        colorBlending.logicOp                               = vk::LogicOp::eCopy;
        colorBlending.attachmentCount                       = 1;
        colorBlending.pAttachments                          = &colorBlendAttachment;
        colorBlending.blendConstants[0]                     = 0.0f;
        colorBlending.blendConstants[1]                     = 0.0f;
        colorBlending.blendConstants[2]                     = 0.0f;
        colorBlending.blendConstants[3]                     = 0.0f;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pSetLayouts            = &m_descriptorSetLayout;

        try
        {
            m_pipelineLayout = m_device->createPipelineLayout(pipelineLayoutInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.stageCount                     = 2;
        pipelineInfo.pStages                        = shaderStages;
        pipelineInfo.pVertexInputState              = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState            = &inputAssembly;
        pipelineInfo.pViewportState                 = &viewportState;
        pipelineInfo.pRasterizationState            = &rasterizer;
        pipelineInfo.pMultisampleState              = &multisampling;
        pipelineInfo.pColorBlendState               = &colorBlending;
        pipelineInfo.pDepthStencilState             = &depthStencil;
        pipelineInfo.layout                         = m_pipelineLayout;
        pipelineInfo.renderPass                     = m_renderPass;
        pipelineInfo.subpass                        = 0;
        pipelineInfo.basePipelineHandle             = nullptr;

        try
        {
            m_graphicsPipeline = m_device->createGraphicsPipeline(nullptr, pipelineInfo).value;
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }

    void VulkanInstance::createCommandPool()
    {
        VulkanUtils::QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        vk::CommandPoolCreateInfo poolInfo = {};
        poolInfo.queueFamilyIndex          = queueFamilyIndices.graphicsFamily.value();

        try
        {
            m_commandPool = m_device->createCommandPool(poolInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    uint32_t VulkanInstance::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void VulkanInstance::createImage(uint32_t                width,
                                     uint32_t                height,
                                     vk::Format              format,
                                     vk::ImageTiling         tiling,
                                     vk::ImageUsageFlags     usage,
                                     vk::MemoryPropertyFlags properties,
                                     vk::Image&              image,
                                     vk::DeviceMemory&       imageMemory)
    {
        // vk::Format texFormat = vk::Format::eR8G8B8A8Srgb;
        vk::ImageCreateInfo imgInfo({},
                                    vk::ImageType::e2D,
                                    format,
                                    {width, height, 1},
                                    1,
                                    1,
                                    vk::SampleCountFlagBits::e1,
                                    tiling,
                                    usage,
                                    vk::SharingMode::eExclusive,
                                    0,
                                    nullptr,
                                    vk::ImageLayout::eUndefined);

        try
        {
            image = m_device->createImage(imgInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create image!");
        }

        vk::MemoryRequirements memRequirements;
        m_device->getImageMemoryRequirements(image, &memRequirements);

        vk::MemoryAllocateInfo allocInfo {};
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        try
        {
            imageMemory = m_device->allocateMemory(allocInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to allocate image memory!");
        }

        m_device->bindImageMemory(image, imageMemory, 0);
    }

    void VulkanInstance::createDepthResources()
    {
        // Find the depth format first
        vk::Format depthFormat = findDepthFormat();
        ;
        vk::ImageCreateInfo depthImgInfo({},
                                         vk::ImageType::e2D,
                                         depthFormat,
                                         {m_swapChainExtent.width, m_swapChainExtent.height, 1},
                                         1,
                                         1,
                                         vk::SampleCountFlagBits::e1,
                                         vk::ImageTiling::eOptimal,
                                         vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                         vk::SharingMode::eExclusive,
                                         0,
                                         nullptr,
                                         vk::ImageLayout::eUndefined);

        createImage(m_swapChainExtent.width,
                    m_swapChainExtent.height,
                    depthFormat,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlags(vk::ImageUsageFlagBits::eDepthStencilAttachment),
                    vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                    m_depthImage,
                    m_depthImageMemory);

        m_depthImageView =
            createImageView(m_depthImage, depthFormat, vk::ImageAspectFlags(vk::ImageAspectFlagBits::eDepth));

        // The undefined layout can be used as initial layout
        // transitionImageLayout(depthImage, depthFormat, vk::ImageLayout::eUndefined,
        // vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    void VulkanInstance::createFramebuffers()
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
        {
            std::array<vk::ImageView, 2> attachments = {m_swapChainImageViews[i], m_depthImageView};

            vk::FramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.renderPass                = m_renderPass;
            framebufferInfo.attachmentCount           = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments              = attachments.data();
            framebufferInfo.width                     = m_swapChainExtent.width;
            framebufferInfo.height                    = m_swapChainExtent.height;
            framebufferInfo.layers                    = 1;

            try
            {
                m_swapChainFramebuffers[i] = m_device->createFramebuffer(framebufferInfo);
            }
            catch (vk::SystemError err)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void VulkanInstance::createBuffer(vk::DeviceSize          size,
                                      vk::BufferUsageFlags    usage,
                                      vk::MemoryPropertyFlags properties,
                                      vk::Buffer&             buffer,
                                      vk::DeviceMemory&       bufferMemory)
    {
        vk::BufferCreateInfo bufferInfo = {};
        bufferInfo.size                 = size;
        bufferInfo.usage                = usage;
        // bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        try
        {
            buffer = m_device->createBuffer(bufferInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        vk::MemoryRequirements memRequirements = m_device->getBufferMemoryRequirements(buffer);

        vk::MemoryAllocateInfo allocInfo = {};
        allocInfo.allocationSize         = memRequirements.size;
        allocInfo.memoryTypeIndex        = findMemoryType(memRequirements.memoryTypeBits, properties);

        try
        {
            bufferMemory = m_device->allocateMemory(allocInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        m_device->bindBufferMemory(buffer, bufferMemory, 0);
    }

    void VulkanInstance::transitionImageLayout(vk::Image       image,
                                               vk::Format      format,
                                               vk::ImageLayout oldLayout,
                                               vk::ImageLayout newLayout)
    {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        // Use an image memory barrier to perform layout transitions, which is primary for synchronization purposes
        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;

        // Not want to use the barrier to transfer queue family ownership
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // barrier.image and barrier.subresourceRange specify the image that is affected and the specific part of
        // the image
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlags(0);
            barrier.dstAccessMask = vk::AccessFlags(vk::AccessFlagBits::eTransferWrite);

            sourceStage      = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eTopOfPipe);
            destinationStage = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eTransfer);
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                 newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlags(vk::AccessFlagBits::eTransferWrite);
            barrier.dstAccessMask = vk::AccessFlags(vk::AccessFlagBits::eShaderRead);

            sourceStage      = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eTransfer);
            destinationStage = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eFragmentShader);
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

        endSingleTimeCommands(commandBuffer);
    }

    vk::CommandBuffer VulkanInstance::beginSingleTimeCommands()
    {
        vk::CommandBufferAllocateInfo allocInfo {};
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandPool        = m_commandPool;
        allocInfo.commandBufferCount = 1;

        vk::CommandBuffer commandBuffer;
        m_device->allocateCommandBuffers(&allocInfo, &commandBuffer);

        vk::CommandBufferBeginInfo beginInfo {};
        beginInfo.flags = vk::CommandBufferUsageFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        commandBuffer.begin(&beginInfo);
        return commandBuffer;
    }

    void VulkanInstance::endSingleTimeCommands(vk::CommandBuffer& commandBuffer)
    {
        commandBuffer.end();

        vk::SubmitInfo submitInfo {};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        m_graphicsQueue.submit(submitInfo, nullptr);
        m_graphicsQueue.waitIdle();

        m_device->freeCommandBuffers(m_commandPool, 1, &commandBuffer);
    }

    void VulkanInstance::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
    {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::BufferImageCopy region {};
        region.bufferOffset      = 0;
        region.bufferRowLength   = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;

        region.imageOffset = vk::Offset3D {0, 0, 0};
        region.imageExtent = vk::Extent3D {width, height, 1};

        // Buffer to image copy operations are enqueued using the following function
        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanInstance::createTextureImage()
    {
        int            texWidth, texHeight, texChannels;
        stbi_uc*       pixels = stbi_load(m_elements->m_texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
        {
            std::cerr << "failed to load texture image!" << std::endl;
        }
        else
        {
            std::cout << "load texture image successfully!" << std::endl;
        }

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;

        try
        {
            createBuffer(imageSize,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         stagingBuffer,
                         stagingBufferMemory);
        }
        catch (std::runtime_error err)
        {
            std::cerr << err.what() << std::endl;
        }

        void* data = m_device->mapMemory(stagingBufferMemory, 0, imageSize);
        memcpy(data, pixels, (size_t)imageSize);
        m_device->unmapMemory(stagingBufferMemory);

        // Clean up the original pixel array
        stbi_image_free(pixels);

        // Create the texture image
        try
        {
            createImage(texWidth,
                        texHeight,
                        vk::Format::eR8G8B8A8Srgb,
                        vk::ImageTiling::eOptimal,
                        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                        vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                        m_textureImage,
                        m_textureImageMemory);
        }
        catch (std::runtime_error err)
        {
            std::cerr << err.what() << std::endl;
        }

        // Copy the staging buffer to the texture image:
        // - Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // - Execute the buffer to image copy operation
        transitionImageLayout(m_textureImage,
                              vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eTransferDstOptimal);
        copyBufferToImage(
            stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        // To be able to start sampling from the texture image in the shader, use one last transition to prepare it
        // for shader access:
        transitionImageLayout(m_textureImage,
                              vk::Format::eR8G8B8A8Srgb,
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createTextureImageView()
    {
        m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb);
    }

    void VulkanInstance::createTextureSampler()
    {
        // Samplers are configured through a vk::SamplerCreateInfo structure,
        // which specifies all filters and transformations that it should apply.
        vk::SamplerCreateInfo samplerInfo {};

        // Specify how to interpolate texels that are magnified or minified
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;

        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy =
            16.0f; // limits the amount of texel samples that can be used to calculate the final color

        // Return black when sampling beyond the image with clamp to border addressing mode.
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;

        // The texels are addressed using the [0, 1) range on all axes.
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp     = vk::CompareOp::eAlways;

        // Use mipmapping
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod     = 0.0f;
        samplerInfo.maxLod     = 0.0f;

        try
        {
            // The sampler is a distinct object that provides an interface to extract colors from a texture.
            m_textureSampler = m_device->createSampler(samplerInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void VulkanInstance::copyBuffer(vk::Buffer& srcBuffer, vk::Buffer& dstBuffer, const vk::DeviceSize& size)
    {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::BufferCopy copyRegion {};
        copyRegion.size = size;
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanInstance::createVertexBuffers()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(m_elements->m_raw_verts.size() * sizeof(Vertex));
        // vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void* data = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, m_elements->m_raw_verts.data(), (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        // Create buffer for the old vertices
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent |
                         vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_vertexBuffer1,
                     m_vertexBufferMemory1);

        // Create buffer for the new vertices
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent |
                         vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_vertexBuffer2,
                     m_vertexBufferMemory2);

        copyBuffer(stagingBuffer, m_vertexBuffer1, bufferSize);
        copyBuffer(stagingBuffer, m_vertexBuffer2, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createIndexBuffer()
    {
        vk::DeviceSize bufferSize =
            static_cast<uint32_t>(m_elements->m_sphere_indices.size() * sizeof(m_elements->m_sphere_indices[0]));
        // vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();;

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void* data = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, m_elements->m_sphere_indices.data(), (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_indexBuffer,
                     m_indexBufferMemory);

        copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createNumVertsBuffer()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(sizeof(int));

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        const int NUM_VERTS = m_elements->m_raw_verts.size();
        void*     data      = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, &NUM_VERTS, (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        // Create buffer for the old vertices
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent |
                         vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_numVertsBuffer,
                     m_numVertsBufferMemory);

        copyBuffer(stagingBuffer, m_numVertsBuffer, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createCellVertArrayBuffer()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(sizeof(int) * m_elements->m_numGridCells * 6);
        // vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();;

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void* data = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, m_cellVertArray, (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_cellVertArrayBuffer,
                     m_cellVertArrayBufferMemory);

        copyBuffer(stagingBuffer, m_cellVertArrayBuffer, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createCellVertCountBuffer()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(sizeof(int) * m_elements->m_numGridCells);
        // vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();;

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void* data = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, m_cellVertCount, (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_cellVertCountBuffer,
                     m_cellVertCountBufferMemory);

        copyBuffer(stagingBuffer, m_cellVertCountBuffer, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createSphereVertsBuffer()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(sizeof(Vertex) * m_elements->m_sphere_verts.size());

        vk::Buffer       stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void* data = m_device->mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, m_elements->m_sphere_verts.data(), (size_t)bufferSize);
        m_device->unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     m_sphereVertsBuffer,
                     m_sphereVertsBufferMemory);

        copyBuffer(stagingBuffer, m_sphereVertsBuffer, bufferSize);

        m_device->destroyBuffer(stagingBuffer);
        m_device->freeMemory(stagingBufferMemory);
    }

    void VulkanInstance::createComputePipeline(const std::vector<unsigned char>& shaderCode, vk::Pipeline& pipelineIdx)
    {
        vk::UniqueShaderModule computeShaderModule = VulkanUtils::createShaderModule(m_device, shaderCode);

        vk::PipelineShaderStageCreateInfo computeShaderStageInfo = {};
        computeShaderStageInfo.flags                             = vk::PipelineShaderStageCreateFlags();
        computeShaderStageInfo.stage                             = vk::ShaderStageFlagBits::eCompute;
        computeShaderStageInfo.module                            = *computeShaderModule;
        computeShaderStageInfo.pName                             = "main";

        vk::DescriptorSetLayoutBinding computeLayoutBindingVertices1 {};
        computeLayoutBindingVertices1.binding            = 0;
        computeLayoutBindingVertices1.descriptorCount    = 1;
        computeLayoutBindingVertices1.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingVertices1.pImmutableSamplers = nullptr;
        computeLayoutBindingVertices1.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding computeLayoutBindingVertices2 {};
        computeLayoutBindingVertices2.binding            = 1;
        computeLayoutBindingVertices2.descriptorCount    = 1;
        computeLayoutBindingVertices2.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingVertices2.pImmutableSamplers = nullptr;
        computeLayoutBindingVertices2.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding computeLayoutBindingCellVertexArray {};
        computeLayoutBindingCellVertexArray.binding            = 2;
        computeLayoutBindingCellVertexArray.descriptorCount    = 1;
        computeLayoutBindingCellVertexArray.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingCellVertexArray.pImmutableSamplers = nullptr;
        computeLayoutBindingCellVertexArray.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding computeLayoutBindingCellVertexCount {};
        computeLayoutBindingCellVertexCount.binding            = 3;
        computeLayoutBindingCellVertexCount.descriptorCount    = 1;
        computeLayoutBindingCellVertexCount.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingCellVertexCount.pImmutableSamplers = nullptr;
        computeLayoutBindingCellVertexCount.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding computeLayoutBindingNumVerts {};
        computeLayoutBindingNumVerts.binding            = 4;
        computeLayoutBindingNumVerts.descriptorCount    = 1;
        computeLayoutBindingNumVerts.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingNumVerts.pImmutableSamplers = nullptr;
        computeLayoutBindingNumVerts.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutBinding computeLayoutBindingSphereVerts {};
        computeLayoutBindingSphereVerts.binding            = 5;
        computeLayoutBindingSphereVerts.descriptorCount    = 1;
        computeLayoutBindingSphereVerts.descriptorType     = vk::DescriptorType::eStorageBuffer;
        computeLayoutBindingSphereVerts.pImmutableSamplers = nullptr;
        computeLayoutBindingSphereVerts.stageFlags         = vk::ShaderStageFlagBits::eCompute;

        std::vector<vk::DescriptorSetLayoutBinding> bindings = {computeLayoutBindingVertices1,
                                                                computeLayoutBindingVertices2,
                                                                computeLayoutBindingCellVertexArray,
                                                                computeLayoutBindingCellVertexCount,
                                                                computeLayoutBindingNumVerts,
                                                                computeLayoutBindingSphereVerts};

        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
        // descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags        = vk::DescriptorSetLayoutCreateFlags();
        descriptorSetLayoutCreateInfo.pNext        = nullptr;
        descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        descriptorSetLayoutCreateInfo.pBindings    = bindings.data();

        try
        {
            m_computeDescriptorSetLayout = m_device->createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        // VkDescriptorPoolSize poolSizes[1];
        std::array<vk::DescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type            = vk::DescriptorType::eStorageBuffer;
        poolSizes[0].descriptorCount = 10;

        vk::DescriptorPoolCreateInfo descriptorPoolInfo = {};
        // descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.flags         = vk::DescriptorPoolCreateFlags();
        descriptorPoolInfo.pNext         = nullptr;
        descriptorPoolInfo.poolSizeCount = 1;
        descriptorPoolInfo.pPoolSizes    = poolSizes.data();
        descriptorPoolInfo.maxSets       = 1;

        try
        {
            m_computeDescriptorPool = m_device->createDescriptorPool(descriptorPoolInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }

        vk::DescriptorSetAllocateInfo allocInfo = {};
        // allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_computeDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_computeDescriptorSetLayout;

        // computeDescriptorSet.resize(1);
        try
        {
            m_computeDescriptorSet = m_device->allocateDescriptorSets(allocInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create compute descriptor sets!");
        }

        // Set descriptor set for the old vertices
        vk::DescriptorBufferInfo computeBufferInfoVertices1 = {};
        computeBufferInfoVertices1.buffer                   = m_vertexBuffer1;
        computeBufferInfoVertices1.offset                   = 0;
        computeBufferInfoVertices1.range = static_cast<uint32_t>(m_elements->m_raw_verts.size() * sizeof(Vertex));

        vk::WriteDescriptorSet writeComputeInfoVertices1 = {};
        writeComputeInfoVertices1.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoVertices1.dstBinding             = 0;
        writeComputeInfoVertices1.descriptorCount        = 1;
        writeComputeInfoVertices1.dstArrayElement        = 0;
        writeComputeInfoVertices1.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoVertices1.pBufferInfo            = &computeBufferInfoVertices1;

        // Set descriptor set for the new vertices
        vk::DescriptorBufferInfo computeBufferInfoVertices2 = {};
        computeBufferInfoVertices2.buffer                   = m_vertexBuffer2;
        computeBufferInfoVertices2.offset                   = 0;
        computeBufferInfoVertices2.range = static_cast<uint32_t>(m_elements->m_raw_verts.size() * sizeof(Vertex));

        vk::WriteDescriptorSet writeComputeInfoVertices2 = {};
        writeComputeInfoVertices2.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoVertices2.dstBinding             = 1;
        writeComputeInfoVertices2.descriptorCount        = 1;
        writeComputeInfoVertices2.dstArrayElement        = 0;
        writeComputeInfoVertices2.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoVertices2.pBufferInfo            = &computeBufferInfoVertices2;

        // Set descriptor set for the cell vertex array
        vk::DescriptorBufferInfo computeBufferInfoCellVertexArray = {};
        computeBufferInfoCellVertexArray.buffer                   = m_cellVertArrayBuffer;
        computeBufferInfoCellVertexArray.offset                   = 0;
        computeBufferInfoCellVertexArray.range = static_cast<uint32_t>(m_elements->m_numGridCells * 6 * sizeof(int));

        vk::WriteDescriptorSet writeComputeInfoCellVertexArray = {};
        writeComputeInfoCellVertexArray.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoCellVertexArray.dstBinding             = 2;
        writeComputeInfoCellVertexArray.descriptorCount        = 1;
        writeComputeInfoCellVertexArray.dstArrayElement        = 0;
        writeComputeInfoCellVertexArray.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoCellVertexArray.pBufferInfo            = &computeBufferInfoCellVertexArray;

        // Set descriptor set for the cell vertex count
        vk::DescriptorBufferInfo computeBufferInfoCellVertexCount = {};
        computeBufferInfoCellVertexCount.buffer                   = m_cellVertCountBuffer;
        computeBufferInfoCellVertexCount.offset                   = 0;
        computeBufferInfoCellVertexCount.range = static_cast<uint32_t>(m_elements->m_numGridCells * sizeof(int));

        vk::WriteDescriptorSet writeComputeInfoCellVertexCount = {};
        writeComputeInfoCellVertexCount.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoCellVertexCount.dstBinding             = 3;
        writeComputeInfoCellVertexCount.descriptorCount        = 1;
        writeComputeInfoCellVertexCount.dstArrayElement        = 0;
        writeComputeInfoCellVertexCount.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoCellVertexCount.pBufferInfo            = &computeBufferInfoCellVertexCount;

        // Set descriptor set for the buffer representing the number of vertices
        vk::DescriptorBufferInfo computeBufferInfoNumVerts = {};
        computeBufferInfoNumVerts.buffer                   = m_numVertsBuffer;
        computeBufferInfoNumVerts.offset                   = 0;
        computeBufferInfoNumVerts.range                    = static_cast<uint32_t>(sizeof(int));

        vk::WriteDescriptorSet writeComputeInfoNumVerts = {};
        writeComputeInfoNumVerts.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoNumVerts.dstBinding             = 4;
        writeComputeInfoNumVerts.descriptorCount        = 1;
        writeComputeInfoNumVerts.dstArrayElement        = 0;
        writeComputeInfoNumVerts.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoNumVerts.pBufferInfo            = &computeBufferInfoNumVerts;

        // Set descriptor set for the buffer representing the sphere vertices
        vk::DescriptorBufferInfo computeBufferInfoSphereVerts = {};
        computeBufferInfoSphereVerts.buffer                   = m_sphereVertsBuffer;
        computeBufferInfoSphereVerts.offset                   = 0;
        computeBufferInfoSphereVerts.range = static_cast<uint32_t>(sizeof(Vertex) * m_elements->m_sphere_verts.size());

        vk::WriteDescriptorSet writeComputeInfoSphereVerts = {};
        writeComputeInfoSphereVerts.dstSet                 = m_computeDescriptorSet[0];
        writeComputeInfoSphereVerts.dstBinding             = 5;
        writeComputeInfoSphereVerts.descriptorCount        = 1;
        writeComputeInfoSphereVerts.dstArrayElement        = 0;
        writeComputeInfoSphereVerts.descriptorType         = vk::DescriptorType::eStorageBuffer;
        writeComputeInfoSphereVerts.pBufferInfo            = &computeBufferInfoSphereVerts;

        std::array<vk::WriteDescriptorSet, 6> writeDescriptorSets = {writeComputeInfoVertices1,
                                                                     writeComputeInfoVertices2,
                                                                     writeComputeInfoCellVertexArray,
                                                                     writeComputeInfoCellVertexCount,
                                                                     writeComputeInfoNumVerts,
                                                                     writeComputeInfoSphereVerts};
        m_device->updateDescriptorSets(
            static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
        std::array<vk::DescriptorSetLayout, 1> descriptorSetLayouts = {m_computeDescriptorSetLayout};

        vk::PipelineLayoutCreateInfo computePipelineLayoutInfo = {};
        computePipelineLayoutInfo.flags                        = vk::PipelineLayoutCreateFlags();
        computePipelineLayoutInfo.setLayoutCount               = static_cast<uint32_t>(descriptorSetLayouts.size());
        computePipelineLayoutInfo.pSetLayouts                  = descriptorSetLayouts.data();
        computePipelineLayoutInfo.pushConstantRangeCount       = 0;
        computePipelineLayoutInfo.pPushConstantRanges          = 0;

        try
        {
            m_computePipelineLayout = m_device->createPipelineLayout(computePipelineLayoutInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        vk::ComputePipelineCreateInfo computePipelineInfo = {};
        computePipelineInfo.flags                         = vk::PipelineCreateFlags();
        computePipelineInfo.stage                         = computeShaderStageInfo;
        computePipelineInfo.layout                        = m_computePipelineLayout;

        try
        {
            pipelineIdx = m_device->createComputePipeline(nullptr, computePipelineInfo).value;
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create compute pipeline!");
        }
    }

    void VulkanInstance::createuniformUboBuffers()
    {
        vk::DeviceSize bufferSize = static_cast<uint32_t>(sizeof(VulkanUtils::UniformBufferObject));

        m_uniformUboBuffers.resize(m_swapChainImages.size());
        m_uniformUboBuffersMemory.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            createBuffer(bufferSize,
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         m_uniformUboBuffers[i],
                         m_uniformUboBuffersMemory[i]);
        }
    }

    void VulkanInstance::createDescriptorPool()
    {
        std::array<vk::DescriptorPoolSize, 2> descriptorPoolSizes {};
        descriptorPoolSizes[0].type            = vk::DescriptorType::eUniformBuffer;
        descriptorPoolSizes[0].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());
        // For the allocation of the combined image sampler
        descriptorPoolSizes[1].type            = vk::DescriptorType::eCombinedImageSampler;
        descriptorPoolSizes[1].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());

        vk::DescriptorPoolCreateInfo poolInfo {};
        poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
        poolInfo.pPoolSizes    = descriptorPoolSizes.data();

        // Specify the maximum number of descriptor sets that may be allocated
        poolInfo.maxSets = static_cast<uint32_t>(m_swapChainImages.size());

        try
        {
            m_descriptorPool = m_device->createDescriptorPool(poolInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void VulkanInstance::createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(m_swapChainImages.size(), m_descriptorSetLayout);
        vk::DescriptorSetAllocateInfo        allocInfo {};
        allocInfo.descriptorPool     = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapChainImages.size());
        allocInfo.pSetLayouts        = layouts.data();

        m_descriptorSets.resize(m_swapChainImages.size());
        try
        {
            m_descriptorSets = m_device->allocateDescriptorSets(allocInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            vk::DescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = m_uniformUboBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(VulkanUtils::UniformBufferObject);

            // The resources for a combined image sampler structure must be specified in a vk::DescriptorImageInfo
            // struct
            vk::DescriptorImageInfo imageInfo {};
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            imageInfo.imageView   = m_textureImageView;
            imageInfo.sampler     = m_textureSampler;

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites {};
            descriptorWrites[0].dstSet = m_descriptorSets[i];
            // Give our uniform buffer binding index 0
            descriptorWrites[0].dstBinding = 0;
            // Specify the first index in the array of descriptors that we want to update.
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType  = vk::DescriptorType::eUniformBuffer;
            // Specify how many array elements you want to update.
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo     = &bufferInfo;

            descriptorWrites[1].dstSet          = m_descriptorSets[i];
            descriptorWrites[1].dstBinding      = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType  = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo      = &imageInfo;

            m_device->updateDescriptorSets(
                static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void VulkanInstance::createCommandBuffers()
    {
        VulkanUtils::QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
        m_commandBuffers.resize(m_swapChainFramebuffers.size());

        vk::CommandBufferAllocateInfo allocInfo = {};
        allocInfo.commandPool                   = m_commandPool;
        allocInfo.level                         = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount            = (uint32_t)m_commandBuffers.size();

        try
        {
            m_commandBuffers = m_device->allocateCommandBuffers(allocInfo);
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < m_commandBuffers.size(); i++)
        {
            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags                      = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

            try
            {
                m_commandBuffers[i].begin(beginInfo);
            }
            catch (vk::SystemError err)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            vk::RenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.renderPass              = m_renderPass;
            renderPassInfo.framebuffer             = m_swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset       = vk::Offset2D {0, 0};
            renderPassInfo.renderArea.extent       = m_swapChainExtent;

            std::array<vk::ClearValue, 2> clearValues {};
            clearValues[0].color           = std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f};
            clearValues[1].depthStencil    = vk::ClearDepthStencilValue {1.0f, 0};
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues    = clearValues.data();

            // Bind the compute pipeline
            // vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelinePhysics);
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipelineResetCellVertex);

            // Bind descriptor sets for compute
            // vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0,
            // 1, &ComputeDescriptorSet, 0, nullptr);
            m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   m_computePipelineLayout,
                                                   0,
                                                   1,
                                                   m_computeDescriptorSet.data(),
                                                   0,
                                                   nullptr);

            // Dispatch the compute kernel, with one thread for each vertex
            m_commandBuffers[i].dispatch(m_elements->m_numGridCells, 1, 1);

            vk::BufferMemoryBarrier computeToComputeBarrier = {};
            computeToComputeBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier.srcQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier.dstQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier.buffer              = m_cellVertCountBuffer;
            computeToComputeBarrier.offset              = 0;
            computeToComputeBarrier.size                = m_elements->m_numGridCells * sizeof(int);

            vk::PipelineStageFlags computeShaderStageFlags_1(vk::PipelineStageFlagBits::eComputeShader);
            vk::PipelineStageFlags computeShaderStageFlags_2(vk::PipelineStageFlagBits::eComputeShader);
            m_commandBuffers[i].pipelineBarrier(computeShaderStageFlags_1,
                                                computeShaderStageFlags_2,
                                                vk::DependencyFlags(),
                                                0,
                                                nullptr,
                                                1,
                                                &computeToComputeBarrier,
                                                0,
                                                nullptr);

            // Bind the compute pipeline
            // vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelinePhysics);
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipelineFillCellVertex);

            // Bind descriptor sets for compute
            // vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0,
            // 1, &ComputeDescriptorSet, 0, nullptr);
            m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   m_computePipelineLayout,
                                                   0,
                                                   1,
                                                   m_computeDescriptorSet.data(),
                                                   0,
                                                   nullptr);

            // Dispatch the compute kernel, with one thread for each vertex
            m_commandBuffers[i].dispatch(uint32_t(m_elements->m_raw_verts.size()), 1, 1);

            vk::BufferMemoryBarrier computeToComputeBarrier1 = {};
            computeToComputeBarrier1.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier1.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier1.srcQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier1.dstQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier1.buffer              = m_cellVertCountBuffer;
            computeToComputeBarrier1.offset              = 0;
            computeToComputeBarrier1.size                = m_elements->m_numGridCells * sizeof(int); // vertexBufferSize

            vk::PipelineStageFlags computeShaderStageFlags_3(vk::PipelineStageFlagBits::eComputeShader);
            vk::PipelineStageFlags computeShaderStageFlags_4(vk::PipelineStageFlagBits::eComputeShader);
            m_commandBuffers[i].pipelineBarrier(computeShaderStageFlags_3,
                                                computeShaderStageFlags_4,
                                                vk::DependencyFlags(),
                                                0,
                                                nullptr,
                                                1,
                                                &computeToComputeBarrier1,
                                                0,
                                                nullptr);

            // Bind the compute pipeline
            // vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelinePhysics);
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipelinePhysics);

            // Bind descriptor sets for compute
            m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   m_computePipelineLayout,
                                                   0,
                                                   1,
                                                   m_computeDescriptorSet.data(),
                                                   0,
                                                   nullptr);

            // Dispatch the compute kernel, with one thread for each vertex
            m_commandBuffers[i].dispatch(uint32_t(m_elements->m_raw_verts.size()), 1, 1);

            vk::BufferMemoryBarrier computeToComputeBarrier2 = {};
            computeToComputeBarrier2.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier2.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToComputeBarrier2.srcQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier2.dstQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToComputeBarrier2.buffer              = m_vertexBuffer2;
            computeToComputeBarrier2.offset              = 0;
            computeToComputeBarrier2.size =
                uint32_t(m_elements->m_raw_verts.size()) * sizeof(Vertex); // vertexBufferSize

            vk::PipelineStageFlags computeShaderStageFlags_5(vk::PipelineStageFlagBits::eComputeShader);
            vk::PipelineStageFlags computeShaderStageFlags_6(vk::PipelineStageFlagBits::eComputeShader);
            m_commandBuffers[i].pipelineBarrier(computeShaderStageFlags_5,
                                                computeShaderStageFlags_6,
                                                vk::DependencyFlags(),
                                                0,
                                                nullptr,
                                                1,
                                                &computeToComputeBarrier2,
                                                0,
                                                nullptr);

            // Bind the compute pipeline
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipelineSphereVertex);

            // Bind descriptor sets for compute
            m_commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                   m_computePipelineLayout,
                                                   0,
                                                   1,
                                                   m_computeDescriptorSet.data(),
                                                   0,
                                                   nullptr);

            // Dispatch the compute kernel, with one thread for each vertex
            m_commandBuffers[i].dispatch(uint32_t(m_elements->m_sphere_verts.size()), 1, 1);

            // Define a memory barrier to transition the vertex buffer from a compute storage object to a vertex
            // input
            vk::BufferMemoryBarrier computeToVertexBarrier = {};
            computeToVertexBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
            computeToVertexBarrier.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
            computeToVertexBarrier.srcQueueFamilyIndex = queueFamilyIndices.computeFamily.value();
            computeToVertexBarrier.dstQueueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
            computeToVertexBarrier.buffer              = m_sphereVertsBuffer;
            computeToVertexBarrier.offset              = 0;
            computeToVertexBarrier.size =
                uint32_t(m_elements->m_sphere_verts.size()) * sizeof(Vertex); // vertexBufferSize

            vk::PipelineStageFlags computeShaderStageFlags(vk::PipelineStageFlagBits::eComputeShader);
            vk::PipelineStageFlags vertexShaderStageFlags(vk::PipelineStageFlagBits::eVertexInput);
            m_commandBuffers[i].pipelineBarrier(computeShaderStageFlags,
                                                vertexShaderStageFlags,
                                                vk::DependencyFlags(),
                                                0,
                                                nullptr,
                                                1,
                                                &computeToVertexBarrier,
                                                0,
                                                nullptr);

            m_commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

            vk::Buffer     vertexBuffers[] = {m_sphereVertsBuffer};
            vk::DeviceSize offsets[]       = {0};
            m_commandBuffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);
            m_commandBuffers[i].bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint32);

            m_commandBuffers[i].bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
            m_commandBuffers[i].drawIndexed(
                static_cast<uint32_t>(uint32_t(m_elements->m_sphere_indices.size())), 1, 0, 0, 0);

            m_commandBuffers[i].endRenderPass();

            try
            {
                m_commandBuffers[i].end();
            }
            catch (vk::SystemError err)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void VulkanInstance::createSyncObjects()
    {
        m_imageAvailableSemaphores.resize(m_max_frames_in_flight);
        m_renderFinishedSemaphores.resize(m_max_frames_in_flight);
        m_inFlightFences.resize(m_max_frames_in_flight);

        try
        {
            for (size_t i = 0; i < m_max_frames_in_flight; i++)
            {
                m_imageAvailableSemaphores[i] = m_device->createSemaphore({});
                m_renderFinishedSemaphores[i] = m_device->createSemaphore({});
                m_inFlightFences[i]           = m_device->createFence({vk::FenceCreateFlagBits::eSignaled});
            }
        }
        catch (vk::SystemError err)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    void VulkanInstance::cleanupSwapChain()
    {
        m_device->destroyImageView(m_depthImageView);
        m_device->destroyImage(m_depthImage);
        m_device->freeMemory(m_depthImageMemory);

        for (auto framebuffer : m_swapChainFramebuffers)
        {
            m_device->destroyFramebuffer(framebuffer);
        }

        m_device->freeCommandBuffers(m_commandPool, m_commandBuffers);

        

        for (auto imageView : m_swapChainImageViews)
        {
            m_device->destroyImageView(imageView);
        }

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            m_device->destroyBuffer(m_uniformUboBuffers[i]);
            m_device->freeMemory(m_uniformUboBuffersMemory[i]);
        }

        
        m_device->destroySwapchainKHR(m_swapChain);
    }

    void VulkanInstance::recreateSwapChain()
    {
        int width = 0, height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_device->waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createDepthResources();
        createFramebuffers();
        createuniformUboBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
    }

    void VulkanInstance::updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime   = std::chrono::high_resolution_clock::now();
        auto        currentTime = std::chrono::high_resolution_clock::now();
        float       time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        VulkanUtils::UniformBufferObject ubo {};
        ubo.model = Matrix4(1.f);
        ubo.view  = m_elements->m_viewMat;
        ubo.proj  = m_elements->getProjectionMatrix(m_swapChainExtent.width, m_swapChainExtent.height);

        void* data = m_device->mapMemory(m_uniformUboBuffersMemory[currentImage], 0, sizeof(ubo));
        memcpy(data, &ubo, sizeof(ubo));
        m_device->unmapMemory(m_uniformUboBuffersMemory[currentImage]);
    }

} // namespace sphexa