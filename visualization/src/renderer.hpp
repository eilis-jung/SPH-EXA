#include "vulkan_instance.h"

namespace sphexa
{
class Renderer
{
    public:
    Renderer() {

    };
    VulkanInstance m_vk;

    void init(GLFWwindow* pWindow, std::shared_ptr<Elements> pElements) {
        m_vk.init(pWindow, pElements);
    }

    void drawFrame() {
        m_vk.drawFrame();
    }

    void idle() {
        m_vk.idle();
    }

    void cleanup(){
        m_vk.cleanup();
    }

};
} // namespace sphexa