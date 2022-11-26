#pragma once

#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <stdexcept>
#include <memory>

#include "vulkan_instance.h"
#include "elements.h"


namespace sphexa
{
    class VulkanWrapper;
    class Window
    {
    private:
        GLFWwindow*    m_pGLFWWindow;
        uint32_t       m_width;
        uint32_t       m_height;
        std::string    m_title;
        std::shared_ptr<Elements> m_elements;

        static bool   m_mouse_left_down;
        static bool   m_mouse_right_down;
        static double m_prev_x;
        static double m_prev_y;
        static double m_mouse_sensitivity;
        static bool   framebufferResized;

        VulkanInstance m_vulkanInstance;

    public:
        Window(uint32_t                    width,
               uint32_t                    height,
               std::string&                title) :
            m_width(width),
            m_height(height), m_title(title), m_pGLFWWindow(nullptr)
        {
        }
        void init()
        {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            m_pGLFWWindow = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
            if (m_pGLFWWindow == nullptr)
                throw std::runtime_error("Failed to create window.");
            glfwSetWindowPos(m_pGLFWWindow, 100, 100);
            glfwSetWindowUserPointer(m_pGLFWWindow, this);
            glfwSetMouseButtonCallback(m_pGLFWWindow, Window::mouseDownCallback);
            glfwSetCursorPosCallback(m_pGLFWWindow, Window::mouseMoveCallback);
            glfwSetFramebufferSizeCallback(m_pGLFWWindow, Window::framebufferResizeCallback);
            m_elements = std::make_shared<Elements> ();
            m_vulkanInstance.init(m_pGLFWWindow, m_elements);
        }

        void loop()
        {
            while (!glfwWindowShouldClose(m_pGLFWWindow))
            {
                glfwPollEvents();
                m_vulkanInstance.drawFrame();
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
}