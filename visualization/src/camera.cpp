#include "camera.h"

using namespace sphexa;

Matrix4 Camera::getProjectionMatrix(size_t width, size_t height)
{
    auto res = glm::perspective(m_fov, width / (float)height, m_zNear, m_zFar);
    res[1][1] *= -1;
    return res;
}

void Camera::updateOrbit(float deltaX, float deltaY, float deltaZ)
{
    m_theta += deltaX;
    m_phi += deltaY;
    m_r = glm::clamp(m_r - deltaZ, m_zNearUserLimit, m_zFarUserLimit);

    float radTheta = glm::radians(m_theta);
    float radPhi   = glm::radians(m_phi);

    Matrix4 rotation = glm::rotate(Matrix4(1.0f), radTheta, Vector3(0.0f, 1.0f, 0.0f)) *
                       glm::rotate(Matrix4(1.0f), radPhi, Vector3(1.0f, 1.0f, 1.0f));
    Matrix4 finalTransform =
        glm::translate(Matrix4(1.0f), Vector3(0.0f)) * rotation * glm::translate(Matrix4(1.0f), Vector3(0.0f, 1.0f, m_r));
    m_viewMat = glm::inverse(finalTransform);

    m_eye = Vector3(-m_viewMat[3][0], -m_viewMat[3][1], -m_viewMat[3][2]);
}