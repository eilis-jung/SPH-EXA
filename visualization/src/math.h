
#pragma once

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

#define DELTA_TIME 0.0001f

namespace sphexa
{
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Matrix4 = glm::mat4;
    
    struct Vertex
    {
        alignas(16) Vector4 position = Vector4(0.f, 0.f, 0.f, 1.f);
        alignas(16) Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && color == rhs.color;
        }
    };

    struct Element
    {
        alignas(16) Vector3 position = Vector3(0.f, 0.f, 0.f);
        alignas(16) Vector3 scale = Vector3(1.f);
        alignas(16) Vector3 velocity = Vector3(0.f, 0.f, 0.f);
        alignas(16) Matrix4 modelMat = Matrix4(1.f);

        void updateModelMat() {
            scale = scale + (float)DELTA_TIME * velocity;
            scale = glm::clamp(scale, Vector3(0.3f), Vector3(20.f));
            modelMat = glm::translate(glm::scale(Matrix4(1.f), scale), position);
        }

        bool operator==(const Element& rhs) const
        {
            return position == rhs.position && scale == rhs.scale;
        }
    };

}