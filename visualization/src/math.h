
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
        Matrix4 scale = glm::scale(Matrix4(1), Vector3(1.f, 1.f, 1.f));
        Vector4 attr1    = Vector4(0.05f, 0.0125f, -1.f, 1.f); // radius, mass, is_running
        Vector4 attr2 = Vector4(-1.f, -1.f, 1.f, 1.f); // neighborMax, hasBrokenBond, d, (null)
        Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && velocity == rhs.velocity && attr1 == rhs.attr1 &&
                   attr2 == rhs.attr2 && color == rhs.color;
        }
    };

    struct Element
    {
        Vector4 position = Vector4(0.f, 0.f, 0.f, 1.f);
        Vector4 velocity = Vector4(0.f, 0.f, 0.f, 1.f);
        Matrix4 scale = glm::scale(Matrix4(1), Vector3(1.f, 1.f, 1.f));
        Vector4 attr1    = Vector4(0.05f, 0.0125f, 1.f, 1.f); // radius, mass, is_running

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && velocity == rhs.velocity && attr1 == rhs.attr1;
        }
    };

}