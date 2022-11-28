#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math.h"

namespace sphexa
{
namespace ElementsGenerator
{
void createCube(std::vector<Vertex>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                 const Vector3& numElementPerSide, const Vector3& offset, const Vector3 color = Vector3(0.f, 0.f, 0.f));

void createSphere(std::vector<Vertex>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                   const int numElementPerSide, const Vector3& offset, const Vector3& color,
                                   const Vector3& InitialVel = Vector3(0.f));
} // namespace ElementsGenerator
} // namespace sphexa