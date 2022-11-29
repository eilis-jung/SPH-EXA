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
    int           m_numElements         = 0;
    int           m_numSide             = 30;
    int           m_numForVisualization = m_numSide * m_numSide * m_numSide;
    float         m_deltaT              = 0.0017f;
    float         r                     = 20.0f;
    float         theta                 = 1.0f;
    float         phi                   = -0.7f;
    Matrix4       m_viewMat             = glm::lookAt(eye, Vector3(2.0f, 2.0f, 2.0f), Vector3(0.0f, 1.0f, 0.0f));
    Vector3       eye                   = Vector3(5.0f, 10.0f, r);
    std::string   m_modelObjPath       = "../../../visualization/assets/models/cube.obj";
    std::string   m_modelPath           = "../../../visualization/assets/models/viking_room.obj";
    std::string   m_texturePath         = "../../../visualization/assets/images/viking_room.png";
#pragma endregion RenderingData

#pragma region PositionData
    std::vector<Vertex>   m_model_vertices;
    std::vector<Element>   m_elements;
    std::vector<uint32_t> m_element_indices;
    std::vector<Vertex>   m_vertices;
    std::vector<uint32_t> m_all_indices;
#pragma endregion PositionData

    void updateOrbit(float deltaX, float deltaY, float deltaZ);

    void updateMovement(bool flags[]);

    Matrix4 getProjectionMatrix(size_t width, size_t height);

    Elements(int numElements)
        : m_numElements(numElements)
    {
        init(numElements);
    };

    void init(int numElements);

private:
    void loadModel(std::string modelPath, Vector4 initColor = Vector4(1.f, 1.f, 1.f, 1.f));
};
} // namespace sphexa