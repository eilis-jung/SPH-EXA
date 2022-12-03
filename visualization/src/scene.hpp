#include "manager.h"
#include "vulkan_instance.h"

namespace sphexa {
    class ElementManager;

    class Scene {
    public:
        ElementManager m_elementManager;
        VulkanInstance m_vk;
        Window m_window;
    }
}