#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "elements.h"
#include "elements_generator.hpp"

using namespace sphexa;


void Elements::updateMovement(bool flags[])
{
    for(int i=0; i<m_elements.size(); i++) {
        if(flags[i] == false)
            m_elements[i].velocity.y = 1.f;
        else
            m_elements[i].velocity.y = 0.f;
        m_elements[i].updateModelMat(m_deltaTime);
    }
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
        Vector3(-1.f, 0., 0.));

    int currVertexInd = 0;
    for (int i = 0; i < m_elements.size(); i++)
    {
        for (int j = 0; j < m_model_vertices.size(); j++)
        {
            Vertex v = m_model_vertices[j];
            m_vertices.push_back(v);
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

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            Vector3 pos = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2]};
            vertex.position   = Vector4(pos, 1.f);
            vertex.color      = initColor;
            vertex.texCoord = Vector2(attrib.texcoords[2*size_t(index.texcoord_index)+0], attrib.texcoords[2*size_t(index.texcoord_index)+1]);
            m_model_vertices.push_back(vertex);
        }
    }

    std::cout << "Model has " << m_model_vertices.size() << " vertices." << std::endl;
}