#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math_vis.h"
#include "camera.h"
#include "meta/singleton.h"

namespace sphexa
{
class Elements
{
public:
#pragma region RenderingData
    float m_deltaTime           = 0.0001f;
    Camera m_camera;

    std::string m_modelObjPath = "../../../visualization/assets/models/cube.obj";
    std::string m_modelPath    = "../../../visualization/assets/models/viking_room.obj";
    std::string m_texturePath  = "../../../visualization/assets/images/redgradient.png";
#pragma endregion RenderingData

#pragma region PositionData
    std::vector<Vertex>   m_model_vertices;
    std::vector<Element>  m_elements;
    std::vector<uint32_t> m_element_indices;
    std::vector<Vertex>   m_vertices;
    std::vector<uint32_t> m_all_indices;
#pragma endregion PositionData

    void updateOrbit(float deltaX, float deltaY, float deltaZ);

    void updateMovement(bool flags[]);

    Matrix4 getProjectionMatrix(size_t width, size_t height);

    Elements(int numElements)
    {
        init(numElements);
    };

    void init(int numElements);

private:
    void loadModel(std::string modelPath, Vector4 initColor = Vector4(1.f, 1.f, 1.f, 1.f));
};
} // namespace sphexa