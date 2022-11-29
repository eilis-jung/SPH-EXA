#include "window.h"

using namespace sphexa;

bool   Window::m_mouse_left_down   = false;
bool   Window::m_mouse_right_down  = false;
double Window::m_prev_x            = 0;
double Window::m_prev_y            = 0;
double Window::m_mouse_sensitivity = 0.1;
bool   Window::framebufferResized  = false;

void Window::mouseDownCallback(GLFWwindow* window, int button, int action, int mods)
{
    double pos_x, pos_y;
    glfwGetCursorPos(window, &pos_x, &pos_y);

    if (button == GLFW_MOUSE_BUTTON_LEFT)
        {if (action == GLFW_PRESS)
            mouseButtonPressLeft(window, pos_x, pos_y);
        else if (action == GLFW_RELEASE)
            mouseButtonReleaseLeft(window, pos_x, pos_y);}
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {if (action == GLFW_PRESS)
            mouseButtonPressRight(window, pos_x, pos_y);
        else if (action == GLFW_RELEASE)
            mouseButtonReleaseRight(window, pos_x, pos_y);}
}

void Window::mouseButtonPressLeft(GLFWwindow* window, double pos_x, double pos_y)
{
    m_mouse_left_down = true;
    glfwGetCursorPos(window, &m_prev_x, &m_prev_y);
    return;
}
void Window::mouseButtonPressRight(GLFWwindow* window, double pos_x, double pos_y)
{
    m_mouse_right_down = true;
    glfwGetCursorPos(window, &m_prev_x, &m_prev_y);
    return;
}
void Window::mouseButtonReleaseLeft(GLFWwindow* window, double pos_x, double pos_y)
{
    m_mouse_left_down = false;
    return;
}
void Window::mouseButtonReleaseRight(GLFWwindow* window, double pos_x, double pos_y)
{
    m_mouse_right_down = false;
    
    return;
}

void Window::mouseMoveCallback(GLFWwindow* window, double pos_x, double pos_y)
{
    if (m_mouse_left_down)
    {
        float delta_x = static_cast<float>((m_prev_x - pos_x) * m_mouse_sensitivity);
        float delta_y = static_cast<float>((m_prev_y - pos_y) * m_mouse_sensitivity);

        m_prev_x = pos_x;
        m_prev_y = pos_y;

        Window* w = static_cast<Window *>(glfwGetWindowUserPointer(window));
        w->m_elements->m_camera.updateOrbit(delta_x, delta_y, 0.0f);
    }
    else if (m_mouse_right_down)
    {
        float delta_z = static_cast<float>((m_prev_y - pos_y) * 0.05f);
        m_prev_y      = pos_y;
        Window* w = static_cast<Window *>(glfwGetWindowUserPointer(window));
        w->m_elements->m_camera.updateOrbit(0.0f, 0.0f, delta_z);
    }
}
void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    framebufferResized = true;
    return;
}
