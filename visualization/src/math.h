
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
        Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && color == rhs.color;
        }
    };

    struct Element
    {
        Vector3 position = Vector3(0.f, 0.f, 0.f);
        Vector3 scale = Vector3(1.f);
        Vector3 velocity = Vector3(0.f, 0.f, 0.f);
        Matrix4 modelMat = Matrix4(1.f);

        void updateModelMat() {
            modelMat = glm::scale(glm::translate(Matrix4(1.f), position), scale);
        }

        bool operator==(const Element& rhs) const
        {
            return position == rhs.position && scale == rhs.scale;
        }
    };

}