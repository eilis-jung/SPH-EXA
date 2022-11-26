#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <tiny_obj_loader.h>
#include "math.h"

namespace sphexa
{
namespace PointsGenerator
{
void createCube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& idxForWholeVertices, const int N_SIDE,
                const Vector3& OFFSET, const Vector3& COLOR);

void createSphere(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& idxForWholeVertices,
                  const int N_SIDE, const Vector3& OFFSET, const Vector3& COLOR,
                  const Vector3& InitialVel = Vector3(0.f));

bool inTanglecube(const Vector3& p);

void createTanglecube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& idxForWholeVertices,
                      const int N_SIDE, const Vector3& OFFSET, const Vector3& COLOR);

bool inHeart(const Vector3& p);

void createHeart(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& idxForWholeVertices, const int N_SIDE,
                 const Vector3& OFFSET, const Vector3& COLOR);

bool inTorus(const Vector3& p);

void createTorus(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& idxForWholeVertices, const int N_SIDE,
                 const Vector3& OFFSET, const Vector3& COLOR);
} // namespace PointsGenerator

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

    void updateOrbit(float deltaX, float deltaY, float deltaZ)
    {
        theta += deltaX;
        phi += deltaY;
        r = glm::clamp(r - deltaZ, 1.0f, 50.0f);

        float radTheta = glm::radians(theta);
        float radPhi   = glm::radians(phi);

        Matrix4 rotation = glm::rotate(Matrix4(1.0f), radTheta, Vector3(0.0f, 1.0f, 0.0f)) *
                           glm::rotate(Matrix4(1.0f), radPhi, Vector3(1.0f, 0.0f, 0.0f));
        Matrix4 finalTransform = glm::translate(Matrix4(1.0f), Vector3(0.0f)) * rotation *
                                 glm::translate(Matrix4(1.0f), Vector3(0.0f, 1.0f, r));
        m_viewMat = glm::inverse(finalTransform);

        eye = Vector3(-m_viewMat[3][0], -m_viewMat[3][1], -m_viewMat[3][2]);
    }
    Matrix4 getProjectionMatrix(size_t width, size_t height)
    {
        auto res = glm::perspective(glm::radians(45.f), width / (float)height, 0.01f, 30.0f);
        res[1][1] *= -1;
        return res;
    }

    void init()
    {
        loadModel(m_sphereObjPath);
        int           idxForWholeVertices = 0;
        const Vector3 OFFSET(0.05f, 0.05f, 0.05f);
        PointsGenerator::createHeart(m_raw_verts, m_raw_indices, idxForWholeVertices, 45, Vector3(2.05f, 3.06f, 2.05f),
                                     Vector3(1.f, 0.f, 0.f));

        int sphereIdx = 0;
        for (int i = 0; i < m_raw_verts.size(); i++)
        {
            auto translation = m_raw_verts[i].position;
            translation.w    = 0.f;
            for (int j = 0; j < m_verts.size(); j++)
            {
                Vertex v = m_verts[j];
                v.position += translation;
                m_sphere_verts.push_back(v);
                m_sphere_indices.push_back(sphereIdx);
                sphereIdx++;
            }
        }
        std::cout << "Number of raw_verts: " << m_raw_verts.size() << std::endl;
        std::cout << "Number of sphereIdx: " << sphereIdx << std::endl;
    }

private:
    void loadModel(std::string modelPath)
    {
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string                      warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
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
                vertex.color      = Vector4(glm::normalize(pos), 1.f);

                m_verts.push_back(vertex);
                m_indices.push_back(m_indices.size());
            }
        }

        std::cout << "Model has " << m_verts.size() << "vertices." << std::endl;
    }
};
} // namespace sphexa