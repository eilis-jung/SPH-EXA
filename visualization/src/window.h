#pragma once

#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <stdexcept>
#include <memory>
#include <thread>

#include "vulkan_instance.h"
#include "elements.h"

namespace sphexa
{
class VulkanWrapper;
class Window
{
private:
    GLFWwindow* m_pGLFWWindow;
    uint32_t    m_width;
    uint32_t    m_height;
    std::string m_title;

    static bool   m_mouse_left_down;
    static bool   m_mouse_right_down;
    static double m_prev_x;
    static double m_prev_y;
    static double m_mouse_sensitivity;
    static bool   framebufferResized;

public:
    VulkanInstance            m_vulkanInstance;
    std::shared_ptr<Elements> m_elements;
    Window(uint32_t width, uint32_t height, std::string&& title)
        : m_width(width)
        , m_height(height)
        , m_title(title)
        , m_pGLFWWindow(nullptr)
    {
    }
    Window(uint32_t width, uint32_t height, std::string& title)
        : m_width(width)
        , m_height(height)
        , m_title(title)
        , m_pGLFWWindow(nullptr)
    {
    }
    void init(int numElements)
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_pGLFWWindow = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(m_pGLFWWindow, this);
        if (m_pGLFWWindow == nullptr) throw std::runtime_error("Failed to create window.");
        glfwSetWindowPos(m_pGLFWWindow, 0, 0);
        glfwSetWindowUserPointer(m_pGLFWWindow, this);
        glfwSetMouseButtonCallback(m_pGLFWWindow, Window::mouseDownCallback);
        glfwSetCursorPosCallback(m_pGLFWWindow, Window::mouseMoveCallback);
        glfwSetFramebufferSizeCallback(m_pGLFWWindow, Window::framebufferResizeCallback);
        m_elements = std::make_shared<Elements>(numElements);
        m_vulkanInstance.init(m_pGLFWWindow, m_elements);
    }

    void loop(bool flags[], bool flags_prev[], std::mutex& mut)
    {
        while (!glfwWindowShouldClose(m_pGLFWWindow))
        {
            // mut.lock();
            glfwPollEvents();
            bool needUpdate = false;
            for (int i = 0; i < 3; i++)
            {
                if (flags[i] != flags_prev[i])
                {
                    std::cout << "Current thread number: " << i << " is finished." << std::endl;
                    flags_prev[i] = flags[i];
                    needUpdate    = true;
                    break;
                }
            }
            if (needUpdate)
            {
                m_elements->updateMovement(flags);
            }
            m_vulkanInstance.drawFrameWithUpdatedVertices();
        }
        m_vulkanInstance.idle();
    }

    GLFWwindow* getPointer() { return m_pGLFWWindow; }

    ~Window()
    {
        m_vulkanInstance.cleanup();
        glfwDestroyWindow(m_pGLFWWindow);
        glfwTerminate();
    }

private:
    static void mouseDownCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseButtonPressLeft(GLFWwindow* window, double pos_x, double pos_y);
    static void mouseButtonPressRight(GLFWwindow* window, double pos_x, double pos_y);
    static void mouseButtonReleaseLeft(GLFWwindow* window, double pos_x, double pos_y);
    static void mouseButtonReleaseRight(GLFWwindow* window, double pos_x, double pos_y);
    static void mouseMoveCallback(GLFWwindow* window, double pos_x, double pos_y);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
} // namespace sphexa