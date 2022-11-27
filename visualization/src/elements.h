#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math.h"
#include "meta/singleton.h"

namespace sphexa
{
class Elements
{
public:
    int m_numGridCells = 10;

#pragma region RenderingData
    int           m_numSide             = 30;
    int           m_numForVisualization = m_numSide * m_numSide * m_numSide;
    float         m_deltaT              = 0.0017f;
    float         m_sphereScaleFactor   = 0.01f;
    float         r                     = 20.0f;
    float         theta                 = 1.0f;
    float         phi                   = -0.7f;
    Matrix4       m_viewMat             = glm::lookAt(eye, Vector3(2.0f, 2.0f, 2.0f), Vector3(0.0f, 1.0f, 0.0f));
    Vector3       eye                   = Vector3(5.0f, 10.0f, r);
    std::string   m_sphereObjPath       = "../../../visualization/assets/models/sphere.obj";
    std::string   m_modelPath           = "../../../visualization/assets/models/viking_room.obj";
    std::string   m_texturePath         = "../../../visualization/assets/images/viking_room.png";
    const Matrix4 m_sphereScaleMatrix =
        glm::scale(Matrix4(1), Vector3(m_sphereScaleFactor, m_sphereScaleFactor, m_sphereScaleFactor));
#pragma endregion RenderingData

#pragma region PositionData
    std::vector<Vertex>   m_verts;
    std::vector<uint32_t> m_indices;
    std::vector<Vertex>   m_raw_verts;
    std::vector<uint32_t> m_raw_indices;
    std::vector<Vertex>   m_sphere_verts;
    std::vector<uint32_t> m_sphere_indices;
#pragma endregion PositionData

    void updateOrbit(float deltaX, float deltaY, float deltaZ);

    void updateMovement(bool flags[]);

    Matrix4 getProjectionMatrix(size_t width, size_t height);

    Elements(int numParticles) { init(numParticles); };

    void init(int numParticles);

private:
    void loadModel(std::string modelPath);
};
} // namespace sphexa