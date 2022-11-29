#include "elements_generator.hpp"

using namespace sphexa;

#define GENERAL_MODEL_SCALING 1

void ElementsGenerator::createCube(std::vector<Element>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                 const Vector3& numElementPerSide, const Vector3& offset)
{
    Vector3 lengths = numElementPerSide * 0.2f;

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

                Element element;
                element.translation = position*0.001f;
                element.scale = Vector3(1.f);
                element.updateModelMat();
                elements.push_back(element);
                elementIndices.push_back(currElementIndex);
                currElementIndex++;
            }
        }
    }

    numElements = currElementIndex;
}
