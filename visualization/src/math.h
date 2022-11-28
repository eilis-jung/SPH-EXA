
#pragma once

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace sphexa
{
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Matrix4 = glm::mat4;
    
    struct Vertex
    {
        Vector4 position = Vector4(0.f, 0.f, 0.f, 1.f);
        Vector4 velocity = Vector4(0.f, 0.f, 0.f, 1.f);
        Vector4 attr1    = Vector4(-1.f, 0.0125f, -1.f, 1.f);
        Vector4 attr2 = Vector4(-1.f, -1.f, 1.f, 1.f);
        Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f); // not used

        bool operator==(const Vertex& other) const
        {
            return position == other.position && velocity == other.velocity && attr1 == other.attr1 &&
                   attr2 == other.attr2 && color == other.color;
        }
    };

}