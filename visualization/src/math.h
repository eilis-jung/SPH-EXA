
#pragma once

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

#define DELTA_TIME 0.0001f
#define MIN_SCALE 0.3f
#define MAX_SCALE 20.0f

namespace sphexa
{
    using Vector2 = glm::vec2;
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Matrix4 = glm::mat4;
    
    struct Vertex
    {
        alignas(16) Vector4 position = Vector4(0.f, 0.f, 0.f, 1.f);
        alignas(16) Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);
        alignas(16) Vector2 texCoord = Vector2(1.f, 0.5f);

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && color == rhs.color;
        }
    };

    struct Element
    {
        alignas(16) Vector4 root = Vector4(0., -1., 0., 1.); // The point that should not move during scaling
        alignas(16) Vector4 position = Vector4(0.f, 0., 0., 1.);
        alignas(16) Vector4 scale = Vector4(1.f);
        alignas(16) Vector4 velocity = Vector4(0.f);
        alignas(16) Matrix4 modelMat = Matrix4(1.f);
        
        void updateModelMat(float & deltaTime) {
            scale = scale + deltaTime * velocity;
            scale = glm::clamp(scale, Vector4(MIN_SCALE), Vector4(MAX_SCALE));
            modelMat = glm::translate(glm::scale(Matrix4(1.f), Vector3(scale)), Vector3(position));
            modelMat = glm::translate(Matrix4(1.f), Vector3(root + position - modelMat * root)) * modelMat;
        }

        bool operator==(const Element& rhs) const
        {
            return position == rhs.position && scale == rhs.scale && velocity == rhs.velocity;
        }
    };

}