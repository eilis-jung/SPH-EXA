#pragma once

#include <stddef.h>
#include "math_vis.h"

namespace sphexa
{
class Camera
{
private:
    // Camera settings
    Vector3 m_eye       = Vector3(0.0f, -1.f, 16.f);
    float   m_r         = 10.0f;
    float   m_theta     = 1.0f;
    float   m_phi       = -0.7f;
    Matrix4 m_viewMat = glm::lookAt(m_eye, Vector3(0.f), Vector3(0.0f, 1.0f, 0.0f));

    // Perspective settings
    float m_fov = glm::radians(90.f);
    float m_zNear = 0.1f;
    float m_zFar = 90.f;

    // UI settings - when user manually set up their camera
    float m_zNearUserLimit = 0.1f;
    float m_zFarUserLimit = 50.f;

public:
    Camera(){};

    Matrix4 getProjectionMatrix(size_t width, size_t height);

    Matrix4 getViewMatrix() {return m_viewMat;};

    void updateOrbit(float deltaX, float deltaY, float deltaZ);
};
} // namespace sphexa