#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math.h"

namespace sphexa
{
namespace ElementsGenerator
{
void createCube(std::vector<Element>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                 const Vector3& numElementPerSide, const Vector3& offset);

void createSphere(std::vector<Element>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                   const int numElementPerSide, const Vector3& offset,
                                   const Vector3& InitialVel = Vector3(0.f));
} // namespace ElementsGenerator
} // namespace sphexa