#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive :enable

#include "constants.h"
#include "structures.h"

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// layout(binding = 1) uniform StatusObject {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
// } ubo;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

void main() {
    gl_Position = ubo.proj * ubo.view * inPosition;
    gl_PointSize = 2.0;
    // fragColor = normalize(inVelocity.xyz);
    // float grayColor = inColor.x * 0.299 + inColor.y * 0.587 + inColor.z + 0.114;
    // fragColor = vec3(grayColor, grayColor, grayColor);
    fragColor = vec3(inColor);
    //fragTexCoord = inTexCoord;
}
