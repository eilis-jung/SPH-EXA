#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive :enable

#include "constants.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;
layout(binding = 2) uniform sampler2D texSampler;

void main() {
    outColor = vec4(fragColor, 1);
    outColor = vec4(1.0, 0.0, 0.0, 1);
}