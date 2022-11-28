#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "elements.h"
#include "elements_generator.hpp"

using namespace sphexa;


void Elements::updateOrbit(float deltaX, float deltaY, float deltaZ)
{
    theta += deltaX;
    phi += deltaY;
    r = glm::clamp(r - deltaZ, 1.0f, 50.0f);

    float radTheta = glm::radians(theta);
    float radPhi   = glm::radians(phi);

    Matrix4 rotation = glm::rotate(Matrix4(1.0f), radTheta, Vector3(0.0f, 1.0f, 0.0f)) *
                       glm::rotate(Matrix4(1.0f), radPhi, Vector3(1.0f, 1.0f, 1.0f));
    Matrix4 finalTransform =
        glm::translate(Matrix4(1.0f), Vector3(0.0f)) * rotation * glm::translate(Matrix4(1.0f), Vector3(0.0f, 1.0f, r));
    m_viewMat = glm::inverse(finalTransform);

    eye = Vector3(-m_viewMat[3][0], -m_viewMat[3][1], -m_viewMat[3][2]);
}

void Elements::updateMovement(bool flags[])
{
    for(int i=0; i<m_elements.size(); i++) {
        if(flags[i])
            m_elements[i].attr1.x = 1.0f;
        else
            m_elements[i].attr1.x = -1.0f;
    }
}

Matrix4 Elements::getProjectionMatrix(size_t width, size_t height)
{
    auto res = glm::perspective(glm::radians(45.f), width / (float)height, 0.01f, 30.0f);
    res[1][1] *= -1;
    return res;
}

void Elements::init(int numElements)
{
    loadModel(m_modelObjPath);
    int           numVertices = 0;
    ElementsGenerator::createCube(
        m_elements, 
        m_element_indices, 
        numVertices,
        Vector3(1.f, 1.f, (float)numElements), 
        Vector3(2.f, 3.f, 2.f));

    int currVertexInd = 0;
    for (int i = 0; i < m_elements.size(); i++)
    {
        auto translation = m_elements[i].position;
        translation.w    = 0.f;
        for (int j = 0; j < m_verts.size(); j++)
        {
            Vertex v = m_verts[j];
            v.position += translation;
            m_all_verts.push_back(v);
            m_all_indices.push_back(currVertexInd);
            currVertexInd++;
        }
    }
    std::cout << "Number of elements: " << m_elements.size() << std::endl;
    std::cout << "Number of all vertices: " << currVertexInd << std::endl;
}

void Elements::loadModel(std::string modelPath, Vector4 initColor)
{
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    bool res = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str());

    if (!res)
    {
        std::cerr << warn + err << std::endl;
        return;
    }

    const float scale = 0.2f;
    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            Vector3 pos = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2]};

            vertex.position   = Vector4(pos, 1.f) * scale;
            vertex.position.w = 1.f;
            // For now all cubes are rendered white
            vertex.color      = initColor;

            m_verts.push_back(vertex);
            m_indices.push_back(m_indices.size());
        }
    }

    std::cout << "Model has " << m_verts.size() << "vertices." << std::endl;
}