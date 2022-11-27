#include "points_generator.hpp"

using namespace sphexa;

void PointsGenerator::createCube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& numVertices,
                                 const Vector3& numsPerSide, const Vector3& offset, const Vector3& color)
{
    Vector3 lengths = numsPerSide / 10.f;

    int idx = numVertices;
    for (int i = 0; i < numsPerSide.x; ++i)
    {
        for (int j = 0; j < numsPerSide.y; ++j)
        {
            for (int k = 0; k < numsPerSide.z; ++k)
            {
                Vector3 position;
                position = Vector3(k * lengths.z / (float)numsPerSide.z, j * lengths.y / (float)numsPerSide.y, i * lengths.x / (float)numsPerSide.x);
                position += offset;

                Vertex v;
                v.position = Vector4(position, 1.f);
                v.color    = Vector4(color, 1.f);
                verts.push_back(v);
                indices.push_back(idx);
                idx++;
            }
        }
    }

    numVertices = idx;
}

void PointsGenerator::createSphere(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& numVertices, 
                                   const int numPerSide, const Vector3& offset, const Vector3& color,
                                   const Vector3& InitialVel)
{
    float       length   = float(numPerSide) / 10.f;
    const float midPoint = float(numPerSide) / 2;

    int idx = numVertices;
    for (int i = 0; i < numPerSide; ++i) // z
    {
        for (int j = 0; j < numPerSide; ++j) // y
        {
            float   y      = j * length / float(numPerSide);
            float   mid_l  = midPoint * length / float(numPerSide);
            float   dy     = j < midPoint ? mid_l - y : y - mid_l;
            float   r2     = mid_l * mid_l - dy * dy;
            Vector3 center = Vector3(mid_l, y, mid_l) + offset;
            for (int k = 0; k < numPerSide; ++k) // x
            {
                Vector3 position;
                position = Vector3(k * length / float(numPerSide), y, i * length / float(numPerSide));
                position += offset;

                Vector3 diff = position - center;
                if (diff.x * diff.x + diff.z * diff.z > r2) continue;

                Vertex v;
                v.position = Vector4(position, 1.f);
                v.color    = Vector4(color, 1.f);
                v.velocity = Vector4(InitialVel, 1.f);
                verts.push_back(v);
                indices.push_back(idx);
                idx++;
            }
        }
    }

    numVertices = idx;
}
