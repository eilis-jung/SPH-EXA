#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "math.h"

namespace sphexa
{
namespace PointsGenerator
{
void createCube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& numVertices, const Vector3& numsPerSide,
                const Vector3& offset, const Vector3& color);

void createSphere(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& numVertices,
                  const int numPerSide, const Vector3& offset, const Vector3& color,
                  const Vector3& InitialVel = Vector3(0.f));

bool inTanglecube(const Vector3& p);

void createTanglecube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices, int& numVertices,
                      const int numPerSide, const Vector3& offset, const Vector3& color);
} // namespace PointsGenerator
} // namespace sphexa