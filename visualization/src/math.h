
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
        // Matrix4 modelMat = glm::scale(Matrix4(1), Vector3(1.f, 1.f, 1.f));
        Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);

        bool operator==(const Vertex& rhs) const
        {
            return position == rhs.position && color == rhs.color;
        }
    };

    struct Element
    {
        Vector3 translation = Vector3(0.f);
        Vector3 scale = Vector3(1.f);
        Vector3 rotation = Vector3(0.f);
        Vector3 velocity = Vector3(0.f, 0.f, 0.f);
        // Vector4 mati0;
        // Vector4 mati1;
        // Vector4 mati2;
        // Vector4 mati3;
        Matrix4 modelMat = Matrix4(1.f);

        bool operator==(const Element& rhs) const
        {
            return translation == rhs.translation && scale == rhs.scale && rotation == rhs.rotation && velocity == rhs.velocity;
        }
        Matrix4 updateModelMat() {
            // modelMat = glm::scale(glm::translate(Matrix4(1.f), translation), scale);
            modelMat = glm::translate(glm::scale(Matrix4(1.f), scale), translation);
            // modelMat = Matrix4(1.f);
            // mati0 = modelMat[0];
            // mati1 = modelMat[1];
            // mati2 = modelMat[2];
            // mati3 = modelMat[3];
            // modelMat = glm::translate(glm::scale(Matrix4(1.f), scale), translation);
            return modelMat;
        }
    };

}