#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math_vis.h"

namespace sphexa
{
namespace ElementsGenerator
{
void createCube(std::vector<Element>& elements, std::vector<uint32_t>& elementIndices, int& numElements,
                                 const Vector3& numElementPerSide, const Vector3& offset);
} // namespace ElementsGenerator
} // namespace sphexa