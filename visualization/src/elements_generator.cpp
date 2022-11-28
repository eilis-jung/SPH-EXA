#include "elements_generator.hpp"

using namespace sphexa;

void ElementsGenerator::createCube(std::vector<Vertex>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                 const Vector3& numElementPerSide, const Vector3& offset, const Vector3 color)
{
    // Color is not used in elements
    Vector3 lengths = numElementPerSide;

    int currElementIndex = numElements;
    for (int i = 0; i < numElementPerSide.x; ++i)
    {
        for (int j = 0; j < numElementPerSide.y; ++j)
        {
            for (int k = 0; k < numElementPerSide.z; ++k)
            {
                Vector3 position;
                position = Vector3(k * lengths.z / (float)numElementPerSide.z, j * lengths.y / (float)numElementPerSide.y, i * lengths.x / (float)numElementPerSide.x);
                position += offset;

                Vertex element;
                element.position = Vector4(position, 1.f);
                element.color    = Vector4(color, 1.f);
                elements.push_back(element);
                elementIndices.push_back(currElementIndex);
                currElementIndex++;
            }
        }
    }

    numElements = currElementIndex;
}

void ElementsGenerator::createSphere(std::vector<Vertex>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                   const int numElementPerSide, const Vector3& offset, const Vector3& color,
                                   const Vector3& InitialVel)
{
    float       length   = float(numElementPerSide) / 10.f;
    const float midPoint = float(numElementPerSide) / 2;

    int idx = numElements;
    for (int i = 0; i < numElementPerSide; ++i) // z
    {
        for (int j = 0; j < numElementPerSide; ++j) // y
        {
            float   y      = j * length / float(numElementPerSide);
            float   mid_l  = midPoint * length / float(numElementPerSide);
            float   dy     = j < midPoint ? mid_l - y : y - mid_l;
            float   r2     = mid_l * mid_l - dy * dy;
            Vector3 center = Vector3(mid_l, y, mid_l) + offset;
            for (int k = 0; k < numElementPerSide; ++k) // x
            {
                Vector3 position;
                position = Vector3(k * length / float(numElementPerSide), y, i * length / float(numElementPerSide));
                position += offset;

                Vector3 diff = position - center;
                if (diff.x * diff.x + diff.z * diff.z > r2) continue;

                Vertex v;
                v.position = Vector4(position, 1.f);
                v.color    = Vector4(color, 1.f);
                v.velocity = Vector4(InitialVel, 1.f);
                elements.push_back(v);
                elementIndices.push_back(idx);
                idx++;
            }
        }
    }

    numElements = idx;
}
